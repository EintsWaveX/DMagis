#include <math.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <raylib.h>
#include <rlgl.h>
#include <GLES3/gl3.h>
#include <emscripten.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <sys/stat.h>

#define SCREEN_WIDTH        1920
#define SCREEN_HEIGHT       1080

#define FONT_H1             56.0
#define FONT_H2             42.0
#define FONT_H3             32.76
#define FONT_H4             28.0
#define FONT_H5             23.24
#define FONT_H6             18.76
#define FONT_P              28.0

#define SPACING_TIGHT       -1
#define SPACING_NORMAL       0
#define SPACING_WIDE         1
#define SPACING_WIDER        2
#define SPACING_EXTRA_WIDE   3
#define SPACING_ULTRA_WIDE   5

#define MAX_INPUT_LENGTH    30
#define CURSOR_BLINK_SPEED  30

#define ENCRYPTION_KEY      "DMagis"
#define ADMIN_SAVEFILE      "AdminSaveFile.dat"
#define SCHOOL_DATABASE     "SchoolDatabase.dat"
#define USER_SAVEFILE       "UserSaveFile.dat"
#define USER_DATABASE       "UserDatabase.dat"
#define MAX_ACCOUNT_LIMIT   495

#define ADMIN_USERNAME      "bT^T[QTxTR\\F"
#define ADMIN_CODENAME      "bxr"
#define ADMIN_PASSWORD_V1   "bxr"
#define ADMIN_PASSWORD_V2   "eGZRGTXxT^T[f\\T[RrGTA\\F"

#define TEXT_MENU_WELCOME_TitleMessage      "Selamat Datang di: D'Magis!"
#define TEXT_MENU_WELCOME_SubtitleMessage   "~ \"Program Makan Siang Gratis\" ;; Parody Daskom Laboratory ~"

#define TEXT_MENU_MAIN_GOVERNMENT_MORNING   "Selamat pagi dan selamat bertugas, Pemerintah setempat..."
#define TEXT_MENU_MAIN_GOVERNMENT_AFTERNOON "Selamat siang dan jangan lupa beristirahat, Pemerintah setempat..."
#define TEXT_MENU_MAIN_GOVERNMENT_EVENING   "Selamat sore dan tetap semangat, Pemerintah setempat..."
#define TEXT_MENU_MAIN_GOVERNMENT_NIGHT     "Selamat malam dan jangan lupa beristirahat, Pemerintah setempat..."

#define TEXT_MENU_MAIN_VENDOR_MORNING       "Selamat pagi dan selamat bertugas," // %s...
#define TEXT_MENU_MAIN_VENDOR_AFTERNOON     "Selamat siang dan jangan lupa beristirahat," // %s...
#define TEXT_MENU_MAIN_VENDOR_EVENING       "Selamat sore dan tetap semangat," // %s...
#define TEXT_MENU_MAIN_VENDOR_NIGHT         "Selamat malam dan jangan lupa beristirahat," // %s...

#define SCHOOL_DAYS         5
#define DISPLAY_PER_PAGE    5

#define MAX_TRIANGLES       30              // Animation purposes...
#define MAX_BGM             4               // BGM purposes...

#define FadeTransition()                    if (Transitioning) { ScreenFade -= 0.05f; if (ScreenFade <= 0) { ScreenFade = 0; Transitioning = false; PreviousMenu = CurrentMenu; CurrentMenu = NextMenu; GoingBack = false; } }
#define min(a, b)                           ((a) < (b) ? (a) : (b))

// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------

EM_JS(double, GetRealTimeJS, (), {
    return Math.floor(Date.now() / 1000);
});

EM_JS(void, syncfs_complete, (), {
    FS.syncfs(false, function (err) {
        if (err) console.error("SYNCFS error:", err);
        else console.log("SYNCFS: Filesystem sync complete.");
    });
});

void SyncFileSystemInit(void* Argument) {
    EM_ASM({
        if (typeof FS === 'undefined' || typeof FS.filesystems === 'undefined' || typeof FS.filesystems.IDBFS === 'undefined') {
            console.error("IDBFS is not available. Make sure you compiled with -s FORCE_FILESYSTEM=1");
            return;
        }

        if (!FS.analyzePath('/data').exists) {
            FS.mkdir('/data');
        }
        FS.mount(FS.filesystems.IDBFS, {}, '/data');
        FS.syncfs(true, function (err) {
            if (err) {
                console.error("SYNCFS error during initial load:", err);
            } else {
                console.log("SYNCFS: Initial load complete.");
            }
        });
    });
}

// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------

typedef enum {
    MENU_WELCOME = 0,
    MENU_WELCOME_GOVERNMENT_SignIn,
    MENU_WELCOME_GOVERNMENT_MainMenuTransition,
    MENU_WELCOME_VENDOR,
    MENU_WELCOME_VENDOR_SignUp,
    MENU_WELCOME_VENDOR_NewAccountConfirmation,
    MENU_WELCOME_VENDOR_SignIn,
    MENU_WELCOME_VENDOR_MainMenuTransition,
    MENU_ABOUT,
    MENU_EXIT,

    MENU_MAIN_GOVERNMENT = 10,
    MENU_MAIN_GOVERNMENT_GetVendorList,
    MENU_MAIN_GOVERNMENT_GetVendorList_DETAILS,
    MENU_MAIN_GOVERNMENT_SchoolManagement,
    MENU_MAIN_GOVERNMENT_SchoolManagement_ADD,
    MENU_MAIN_GOVERNMENT_SchoolManagement_EDIT,
    MENU_MAIN_GOVERNMENT_SchoolManagement_DELETE,
    MENU_MAIN_GOVERNMENT_BilateralManagement,
    MENU_MAIN_GOVERNMENT_BilateralManagement_CHOOSING,
    MENU_MAIN_GOVERNMENT_GetBilateralList,
    MENU_MAIN_GOVERNMENT_ConfirmBudgetPlan,
    MENU_MAIN_GOVERNMENT_ConfirmBudgetPlan_CHOOSING,

    MENU_MAIN_VENDOR = 30,
    MENU_MAIN_VENDOR_GetAffiliatedSchoolData,
    MENU_MAIN_VENDOR_MenusManagement,
    MENU_MAIN_VENDOR_MenusManagement_ADD,
    MENU_MAIN_VENDOR_MenusManagement_EDIT,
    MENU_MAIN_VENDOR_MenusManagement_DELETE,
    MENU_MAIN_VENDOR_GetDailyMenuStatusList,
    MENU_MAIN_VENDOR_SubmitBudgetPlan,

    MENU_SUBMAIN_SortingOptions = 99
} MenuState;

typedef enum {
    MONDAY, TUESDAY, WEDNESDAY, THURSDAY, FRIDAY
} SchoolDays;

typedef enum {
    SORT_NORMAL, SORT_A_TO_Z, SORT_Z_TO_A, SORT_LOW_TO_HIGH, SORT_HIGH_TO_LOW
} SortingMode;

typedef struct {
    double LastTime;
    double CurrentTime;
    int CooldownTime;
    int RemainingTime;
    int SignInChance;
    bool AllValid;
} SetTime;

typedef struct {
    Font FontName;
    const char *TextFill;
    Vector2 TextPosition;
    Vector2 FontData; // FontSize, Spacing
} TextObject;

typedef struct {
    Rectangle Box;
    char Text[MAX_INPUT_LENGTH + 1]; // 30 chars + null terminator
    char LongText[(MAX_INPUT_LENGTH + 70) + 1]; // 100 chars + null terminator
    char MaskedPassword[MAX_INPUT_LENGTH + 1]; // 30 chars + null terminator
    bool IsActive;
    bool IsValid; // Used for validation (e.g., highlight empty input on submit)
} InputBox;

typedef struct {
    // char Description[MAX_INPUT_LENGTH + 70 + 1];
    char Carbohydrate[MAX_INPUT_LENGTH + 1];
    char Protein[MAX_INPUT_LENGTH + 1];
    char Vegetables[MAX_INPUT_LENGTH + 1];
    char Fruits[MAX_INPUT_LENGTH + 1];
    char Milk[MAX_INPUT_LENGTH + 1];
} HealthyPerfectFoods;

typedef struct {
    char Name[MAX_INPUT_LENGTH + 1];
    char AffiliatedVendor[MAX_INPUT_LENGTH + 1];
    char Students[4 + 1];
} SchoolDatabase;

typedef struct {
    char Username[MAX_INPUT_LENGTH + 1], Password[MAX_INPUT_LENGTH + 1];
    SchoolDatabase AffiliatedSchoolData;
    HealthyPerfectFoods Menus[SCHOOL_DAYS];
    int BudgetPlanConfirmation[SCHOOL_DAYS]; // 2: OK, 1: ?, 0: -, -1: !
    int SaveSchoolDataIndex;
} VendorDatabase;

typedef struct {
    VendorDatabase Vendor; // Assigned to ONE (1) school only at a time
    int VendorID;
} VendorLoggedInData;

// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------

typedef struct {
    Vector2 position;
    float size;
    float speed;
    float angle;          // Current rotation angle
    float rotationSpeed;  // Degrees per frame
    Color c1, c2, c3;
} Triangle;

Color RandomColor() {
    return (Color){ GetRandomValue(50, 255), GetRandomValue(50, 255), GetRandomValue(50, 255), 155 };
}

Color BlendWithWhite(Color color, float factor) {
    if (factor < 0) factor = 0;
    if (factor > 1) factor = 1;

    Color result;
    result.r = (unsigned char)(color.r * (1.0f - factor) + 255 * factor);
    result.g = (unsigned char)(color.g * (1.0f - factor) + 255 * factor);
    result.b = (unsigned char)(color.b * (1.0f - factor) + 255 * factor);
    result.a = color.a;
    return result;
}


// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------

Font GIFont;
Sound ButtonClickSFX;
MenuState PreviousMenu, CurrentMenu, NextMenu;
SortingMode SortingStatus;
float CurrentFrameTime = 0, OccuringLastCharacter = 0;
float HoverTransition[1024] = { 0 };    // 1022-th index is for a previous menu button animation.
                                        // 1023-th index is for signing out button animation.

VendorDatabase OriginalVendors[MAX_ACCOUNT_LIMIT], Vendors[MAX_ACCOUNT_LIMIT];
VendorLoggedInData User;
int UserAccountIndex = 0;
bool UserAccountExists = false, SuccessfulWriteUserAccount = false;

VendorDatabase RequestedBPFromVendors[MAX_ACCOUNT_LIMIT];
SchoolDatabase OriginalSchools[MAX_ACCOUNT_LIMIT], Schools[MAX_ACCOUNT_LIMIT], OnlyNonAffiliatedSchools[MAX_ACCOUNT_LIMIT], OnlyAffiliatedSchools[MAX_ACCOUNT_LIMIT];
int CurrentPages[6] = { 0, 0, 0, 0, 0, 0 }, ShownDataOnPage = 0;
int RegisteredVendors = 0, RegisteredSchools = 0;
int ReadVendors = 0, ReadSchools = 0, ReadOnlyNonAffiliatedSchools = 0, ReadOnlyAffiliatedSchools = 0, ReadRequestedBPFromVendors = 0;
int UsersOnPage = 0, SchoolsOnPage = 0, OnlyNonAffiliatedSchoolsOnPage = 0, OnlyAffiliatedSchoolsOnPage = 0, RequestedBPFromVendorsOnPage = 0;
char OldSchoolName[MAX_INPUT_LENGTH + 1] = { 0 }, NewSchoolName[MAX_INPUT_LENGTH + 1] = { 0 };
char GetTemporaryVendorUsername[MAX_INPUT_LENGTH + 1] = { 0 };
int GetTemporaryVendorIndex = -1;
int VendorMenuIndex = -1;

SetTime ST_Admin = {
    .AllValid = false,
    .CooldownTime = 15,
    .CurrentTime = 0.0,
    .LastTime = 0.0,
    .RemainingTime = 0,
    .SignInChance = 3
};
SetTime ST_User = {
    .AllValid = false,
    .CooldownTime = 15,
    .CurrentTime = 0.0,
    .LastTime = 0.0,
    .RemainingTime = 0,
    .SignInChance = 3
};
bool FirstRun = true, CheckForSignInClick = false, OpenAccessForAdmin = false, OpenAccessForUser = false;

InputBox SchoolsManagementInputBox_ADD[2] = { 0 }, SchoolsManagementInputBox_EDIT[2] = { 0 }, SchoolsManagementInputBox_DELETE[3] = { 0 };
InputBox MenusManagementInputBox_ADD[5] = { 0 }, MenusManagementInputBox_EDIT[5] = { 0 }, MenusManagementInputBox_DELETE[5] = { 0 };
InputBox GoToPageInputBox[6];

int ActiveBox = -1;     // -1 means no active input box
int FrameCounter = 0;   // Used for blinking cursor effect
float ScreenFade = 0;
bool Transitioning = false, GoingBack = false;

bool HasClicked[10] = {
    false, false, false, false, // ADMIN_SIGNIN, USER_SIGNUP_1, USER_SIGNUP_2, USER_SIGNIN
    false, false, false,        // SCHOOL_ADD, SCHOOL_EDIT, SCHOOL_DELETE
    false, false, false         // MENU_ADD, MENU_EDIT, MENU_DELETE
};
bool SchoolDataAlreadyExists = false;
bool SchoolsManagementInputBox_ADD_AllValid = false;
bool SchoolsManagementInputBox_EDIT_AllValid = false;
bool MenusManagementInputBox_ADD_AllValid = false;
bool MenusManagementInputBox_EDIT_AllValid = false;

bool IsCardTransitioning = false;
int CurrentCardMenuProfile = 0;
float SetCardPositionInX = 0; // Start off-screen left
float TargetX = 0;
int TransitionDirection = 0; // 1 = right, -1 = left

// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------

// Lerp animation.
float CustomLerp(float a, float b, float t) { return a + t * (b - a); }

// Get encryption key.
int GetEncryptionKey(char* StringCode) {
    int EncryptionKey = 0;
    for (size_t i = 0; i < strlen(StringCode); i++) { EncryptionKey += StringCode[i]; }
    return EncryptionKey;
}

// Encrypt the given string.
void EXDRO(char* Text, int EncryptionKey) { for (size_t i = 0; i < strlen(Text); i++) { Text[i] ^= EncryptionKey; } }

int A_to_Z_SortingMode(const void* A, const void* B) {
    if (CurrentMenu == MENU_MAIN_GOVERNMENT_BilateralManagement || CurrentMenu == MENU_MAIN_GOVERNMENT_GetVendorList) {
        return strcasecmp(((VendorDatabase *)A)->Username, ((VendorDatabase *)B)->Username);
    } else {
        return strcasecmp(((SchoolDatabase *)A)->Name, ((SchoolDatabase *)B)->Name);
    }
}
int Z_to_A_SortingMode(const void* A, const void* B) {
    if (CurrentMenu == MENU_MAIN_GOVERNMENT_BilateralManagement || CurrentMenu == MENU_MAIN_GOVERNMENT_GetVendorList) {
        return strcasecmp(((VendorDatabase *)B)->Username, ((VendorDatabase *)A)->Username);
    } else {
        return strcasecmp(((SchoolDatabase *)B)->Name, ((SchoolDatabase *)A)->Name);
    }
}
int LOW_to_HIGH_SortingMode(const void* A, const void* B) {
    if (CurrentMenu == MENU_MAIN_GOVERNMENT_BilateralManagement || CurrentMenu == MENU_MAIN_GOVERNMENT_GetVendorList) {
        return atoi(((VendorDatabase *)A)->AffiliatedSchoolData.Students) - atoi(((VendorDatabase *)B)->AffiliatedSchoolData.Students);
    } else if (CurrentMenu == MENU_MAIN_GOVERNMENT_ConfirmBudgetPlan) {
        int BP_A = 0, BP_B = 0;
        for (int i = 0; i < SCHOOL_DAYS; i++) {
            if (((VendorDatabase *)A)->BudgetPlanConfirmation[i] == 1) BP_A++;
            if (((VendorDatabase *)B)->BudgetPlanConfirmation[i] == 1) BP_B++;
        }

        return BP_A - BP_B;
    } else {
        return atoi(((SchoolDatabase *)A)->Students) - atoi(((SchoolDatabase *)B)->Students);
    }
    
}
int HIGH_to_LOW_SortingMode(const void* A, const void* B) {
    if (CurrentMenu == MENU_MAIN_GOVERNMENT_BilateralManagement || CurrentMenu == MENU_MAIN_GOVERNMENT_GetVendorList) {
        return atoi(((VendorDatabase *)B)->AffiliatedSchoolData.Students) - atoi(((VendorDatabase *)A)->AffiliatedSchoolData.Students);
    } else if (CurrentMenu == MENU_MAIN_GOVERNMENT_ConfirmBudgetPlan) {
        int BP_A = 0, BP_B = 0;
        for (int i = 0; i < SCHOOL_DAYS; i++) {
            if (((VendorDatabase *)A)->BudgetPlanConfirmation[i] == 1) BP_A++;
            if (((VendorDatabase *)B)->BudgetPlanConfirmation[i] == 1) BP_B++;
        }

        return BP_B - BP_A;
    } else {
        return atoi(((SchoolDatabase *)B)->Students) - atoi(((SchoolDatabase *)A)->Students);
    }
}

// Function to get current time 24-hour format clock
int GetTimeOfDay(void) {
    time_t currentTime;
    struct tm *localTime;

    time(&currentTime);
    localTime = localtime(&currentTime);

    int hour = localTime->tm_hour;
    if (hour >= 5 && hour < 12) {
        // "Good Morning! ☀️
        return 1;
    } else if (hour >= 12 && hour < 17) {
        // "Good Afternoon! 🌤️
        return 2;
    } else if (hour >= 17 && hour < 20) {
        // "Good Evening! 🌆
        return 3;
    } else {
        // "Good Night! 🌙
        return 4;
    }
}

int GetDaysOfWeek(void) {
    time_t currentTime;
    struct tm *localTime;

    time(&currentTime);
    localTime = localtime(&currentTime);

    return localTime->tm_wday;
}

void GetWholeDate(struct tm** LocalTime) {
    time_t currentTime;

    time(&currentTime);
    *LocalTime = localtime(&currentTime);
}

// Function to get the current timestamp
void GetCurrentRealTime(SetTime* ST) { ST->CurrentTime = GetRealTimeJS(); }

// Function to get the last sign-in time from the file
void GetLastSignInTime(SetTime* ST, const char *FileName) {
    FILE *file = fopen(FileName, "rb");
    if (!file) {
        ST->AllValid = false;
        ST->CooldownTime = 15;
        ST->CurrentTime = 0;
        ST->LastTime = 0;
        ST->RemainingTime = 0;
        ST->SignInChance = 3; // Default sign-in chances
        return;
    }

    fread(ST, sizeof(SetTime), 1, file);
    fclose(file);
}

// Function to update the last sign-in time in the file
void UpdateLastSignInTime(SetTime* ST, const char *FileName) {
    FILE *file = fopen(FileName, "wb");
    if (!file) return;

    GetCurrentRealTime(ST); // Update current time for that user type
    fwrite(ST, sizeof(SetTime), 1, file);
    fclose(file);

    syncfs_complete();
}

// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------

void DrawRotatedGradientTriangle(Triangle t) {
    float halfSize = t.size / 2;

    Vector2 v1 = { -halfSize,  halfSize };
    Vector2 v2 = {  halfSize,  halfSize };
    Vector2 v3 = { 0, -halfSize };

    float rad = DEG2RAD * t.angle;
    float cosA = cosf(rad);
    float sinA = sinf(rad);

    Vector2 rv1 = {
        t.position.x + v1.x * cosA - v1.y * sinA,
        t.position.y + v1.x * sinA + v1.y * cosA
    };
    Vector2 rv2 = {
        t.position.x + v2.x * cosA - v2.y * sinA,
        t.position.y + v2.x * sinA + v2.y * cosA
    };
    Vector2 rv3 = {
        t.position.x + v3.x * cosA - v3.y * sinA,
        t.position.y + v3.x * sinA + v3.y * cosA
    };

    rlBegin(RL_TRIANGLES);
    rlColor4ub(t.c1.r, t.c1.g, t.c1.b, t.c1.a); rlVertex2f(rv1.x, rv1.y);
    rlColor4ub(t.c2.r, t.c2.g, t.c2.b, t.c2.a); rlVertex2f(rv2.x, rv2.y);
    rlColor4ub(t.c3.r, t.c3.g, t.c3.b, t.c3.a); rlVertex2f(rv3.x, rv3.y);
    rlEnd();
}

// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------

void SyncVendorsFromOriginalVendorData(VendorDatabase* VendorData, VendorDatabase* OriginalVendorData) {
    memcpy(VendorData, OriginalVendorData, sizeof(VendorDatabase) * MAX_ACCOUNT_LIMIT);
}

void SyncSchoolsFromOriginalSchoolData(SchoolDatabase* SchoolData, SchoolDatabase* OriginalSchoolData) {
    memcpy(SchoolData, OriginalSchoolData, sizeof(SchoolDatabase) * MAX_ACCOUNT_LIMIT);
}

void WriteUserAccount(const VendorDatabase* OriginalVendorData) {
    FILE *file = fopen("/data/"USER_DATABASE, "wb");
    if (!file) return;

    size_t vendorCount = 0;
    for (size_t i = 0; i < MAX_ACCOUNT_LIMIT; i++) {
        if (OriginalVendorData[i].Username[0] != '\0') {
            vendorCount++;
        }
    }

    fwrite(&vendorCount, sizeof(size_t), 1, file);
    fwrite(OriginalVendorData, sizeof(VendorDatabase), vendorCount, file);
    fclose(file);

    syncfs_complete();
}

int ReadUserAccount(VendorDatabase* VendorData) {
    FILE *file = fopen("/data/"USER_DATABASE, "rb");
    if (!file) return 0;

    size_t vendorCount = 0;
    fread(&vendorCount, sizeof(size_t), 1, file);  // Read stored vendor count

    if (vendorCount > MAX_ACCOUNT_LIMIT) {
        fclose(file);
        return 0;  // Prevent overflow if data is corrupt
    }

    fread(VendorData, sizeof(VendorDatabase), vendorCount, file);
    fclose(file);

    // Sorting logic
    if (SortingStatus != SORT_NORMAL) {
        int (*compareFunc)(const void*, const void*) = NULL;
        switch (SortingStatus) {
            case SORT_A_TO_Z: compareFunc = A_to_Z_SortingMode; break;
            case SORT_Z_TO_A: compareFunc = Z_to_A_SortingMode; break;
            case SORT_LOW_TO_HIGH: compareFunc = LOW_to_HIGH_SortingMode; break;
            case SORT_HIGH_TO_LOW: compareFunc = HIGH_to_LOW_SortingMode; break;
            default: break;
        }

        if (compareFunc) {
            qsort(VendorData, vendorCount, sizeof(VendorDatabase), compareFunc);
        }
    }

    return vendorCount;
}

int ReadUsersWithPagination(int PageNumber, VendorDatabase* VendorData) {
    VendorDatabase AllUsers[MAX_ACCOUNT_LIMIT];
    int totalUsers = ReadUserAccount(AllUsers);

    if (totalUsers == 0 || PageNumber * DISPLAY_PER_PAGE >= totalUsers) {
        return 0;
    }

    size_t startIndex = PageNumber * DISPLAY_PER_PAGE;
    size_t usersOnPage = ((size_t)totalUsers > startIndex) ? min(DISPLAY_PER_PAGE, totalUsers - startIndex) : 0;

    memcpy(VendorData, &AllUsers[startIndex], usersOnPage * sizeof(VendorDatabase));

    return usersOnPage;
}

int ReadRequestingBPVendors(VendorDatabase* VendorData) {
    FILE *file = fopen("/data/"USER_DATABASE, "rb");
    if (!file) return 0;

    VendorDatabase AllVendors[MAX_ACCOUNT_LIMIT];
    size_t totalVendors = ReadUserAccount(AllVendors);
    fclose(file);

    size_t BPCount = 0;
    for (size_t i = 0; i < totalVendors; i++) {
        if (AllVendors[i].BudgetPlanConfirmation[0] == 1 || 
            AllVendors[i].BudgetPlanConfirmation[1] == 1 || 
            AllVendors[i].BudgetPlanConfirmation[2] == 1 || 
            AllVendors[i].BudgetPlanConfirmation[3] == 1 || 
            AllVendors[i].BudgetPlanConfirmation[4] == 1) {
            
            VendorData[BPCount++] = AllVendors[i];
        }
    }

    if (SortingStatus != SORT_NORMAL) {
        int (*compareFunc)(const void*, const void*) = NULL;
        switch (SortingStatus) {
            case SORT_A_TO_Z: compareFunc = A_to_Z_SortingMode; break;
            case SORT_Z_TO_A: compareFunc = Z_to_A_SortingMode; break;
            case SORT_LOW_TO_HIGH: compareFunc = LOW_to_HIGH_SortingMode; break;
            case SORT_HIGH_TO_LOW: compareFunc = HIGH_to_LOW_SortingMode; break;
            default: break;
        }
        
        if (compareFunc) {
            qsort(VendorData, BPCount, sizeof(VendorDatabase), compareFunc);
        }
    }

    return BPCount;
}

int ReadRequestingBPVendorsWithPagination(int PageNumber, VendorDatabase* VendorData) {
    VendorDatabase BPVendors[MAX_ACCOUNT_LIMIT];
    int BPCount = ReadRequestingBPVendors(BPVendors);

    int TotalPages = (BPCount + DISPLAY_PER_PAGE - 1) / DISPLAY_PER_PAGE;
    if (PageNumber < 0 || PageNumber >= TotalPages) return 0; 

    size_t startIdx = PageNumber * DISPLAY_PER_PAGE;
    size_t collected = 0;

    for (size_t i = startIdx; i < (size_t)BPCount && collected < DISPLAY_PER_PAGE; i++) {
        VendorData[collected++] = BPVendors[i];
    }

    return collected;
}

void WriteSchoolsData(const SchoolDatabase* OriginalSchoolData) {
    FILE *file = fopen("/data/"SCHOOL_DATABASE, "wb");
    if (!file) return;

    size_t schoolCount = 0;
    for (size_t i = 0; i < MAX_ACCOUNT_LIMIT; i++) {
        if (OriginalSchoolData[i].Name[0] != '\0') {
            schoolCount++;
        }
    }

    fwrite(&schoolCount, sizeof(size_t), 1, file);
    fwrite(OriginalSchoolData, sizeof(SchoolDatabase), schoolCount, file);
    fclose(file);

    syncfs_complete();
}

int ReadSchoolsData(SchoolDatabase* SchoolData) {
    static bool isOriginalPopulated = false;  // Ensure we populate OriginalSchools only once

    FILE *file = fopen("/data/"SCHOOL_DATABASE, "rb");
    if (!file) return 0;

    size_t schoolCount = 0;
    fread(&schoolCount, sizeof(size_t), 1, file);  // Read stored school count

    if (schoolCount > MAX_ACCOUNT_LIMIT) {
        fclose(file);
        return 0;  // Prevent overflow if data is corrupt
    }

    // Populate OriginalSchools only once to prevent it from getting overwritten
    if (!isOriginalPopulated) {
        fread(OriginalSchools, sizeof(SchoolDatabase), schoolCount, file);
        isOriginalPopulated = true;  // Mark as populated
    } else {
        fseek(file, sizeof(SchoolDatabase) * schoolCount, SEEK_CUR);  // Skip over data
    }

    fclose(file);

    // Copy the original unsorted data into SchoolData
    memcpy(SchoolData, OriginalSchools, sizeof(SchoolDatabase) * schoolCount);

    // If SortingStatus is NOT NORMAL, sort the copy of the data
    if (SortingStatus != SORT_NORMAL) {
        int (*compareFunc)(const void*, const void*) = NULL;
        switch (SortingStatus) {
            case SORT_A_TO_Z: compareFunc = A_to_Z_SortingMode; break;
            case SORT_Z_TO_A: compareFunc = Z_to_A_SortingMode; break;
            case SORT_LOW_TO_HIGH: compareFunc = LOW_to_HIGH_SortingMode; break;
            case SORT_HIGH_TO_LOW: compareFunc = HIGH_to_LOW_SortingMode; break;
            default: break;
        }

        if (compareFunc) {
            qsort(SchoolData, schoolCount, sizeof(SchoolDatabase), compareFunc);
        }
    }

    return schoolCount;
}

int ReadSchoolsWithPagination(int PageNumber, SchoolDatabase* SchoolData) {
    SchoolDatabase allSchools[MAX_ACCOUNT_LIMIT];
    int totalSchools = ReadSchoolsData(allSchools);

    if (totalSchools == 0 || PageNumber * DISPLAY_PER_PAGE >= totalSchools) {
        return 0;
    }

    int startIdx = PageNumber * DISPLAY_PER_PAGE;
    int endIdx = startIdx + DISPLAY_PER_PAGE;
    if (endIdx > totalSchools) endIdx = totalSchools;

    memcpy(SchoolData, &allSchools[startIdx], (endIdx - startIdx) * sizeof(SchoolDatabase));

    return endIdx - startIdx;
}

int ReadNonAffiliatedSchools(SchoolDatabase* SchoolData) {
    SchoolDatabase allSchools[MAX_ACCOUNT_LIMIT];
    int totalSchools = ReadSchoolsData(allSchools);

    size_t nonAffiliatedCount = 0;
    for (size_t i = 0; i < (size_t)totalSchools; i++) {
        if (allSchools[i].AffiliatedVendor[0] == '\0') {
            SchoolData[nonAffiliatedCount++] = allSchools[i];
        }
    }

    return nonAffiliatedCount;
}

int ReadNonAffiliatedSchoolsWithPagination(int PageNumber, SchoolDatabase* SchoolData) {
    SchoolDatabase allSchools[MAX_ACCOUNT_LIMIT];
    int nonAffiliatedCount = ReadNonAffiliatedSchools(allSchools);

    if ((size_t)(PageNumber * DISPLAY_PER_PAGE) >= (size_t)nonAffiliatedCount) return 0;

    size_t startIdx = PageNumber * DISPLAY_PER_PAGE;
    size_t endIdx = startIdx + DISPLAY_PER_PAGE;
    if (endIdx > (size_t)nonAffiliatedCount) endIdx = nonAffiliatedCount;

    memcpy(SchoolData, &allSchools[startIdx], (endIdx - startIdx) * sizeof(SchoolDatabase));

    return endIdx - startIdx;
}

int ReadAffiliatedSchools(SchoolDatabase* SchoolData) {
    SchoolDatabase allSchools[MAX_ACCOUNT_LIMIT];
    int totalSchools = ReadSchoolsData(allSchools);  // Already sorted

    size_t affiliatedCount = 0;
    for (size_t i = 0; i < (size_t)totalSchools; i++) {
        if (allSchools[i].AffiliatedVendor[0] != '\0') {
            SchoolData[affiliatedCount] = allSchools[i];  // Direct copy, keeps order
            affiliatedCount++;
        }
    }

    return affiliatedCount;
}

int ReadAffiliatedSchoolsWithPagination(int PageNumber, SchoolDatabase* SchoolData) {
    SchoolDatabase allSchools[MAX_ACCOUNT_LIMIT];
    int affiliatedCount = ReadAffiliatedSchools(allSchools);  // Already sorted

    if ((size_t)(PageNumber * DISPLAY_PER_PAGE) >= (size_t)affiliatedCount) return 0;

    size_t startIdx = PageNumber * DISPLAY_PER_PAGE;
    size_t endIdx = startIdx + DISPLAY_PER_PAGE;
    if (endIdx > (size_t)affiliatedCount) endIdx = affiliatedCount;

    memcpy(SchoolData, &allSchools[startIdx], (endIdx - startIdx) * sizeof(SchoolDatabase));

    return endIdx - startIdx;
}

// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------

TextObject CreateTextObject(Font GetFont, const char* TextFill, float FontSize, float Spacing, Vector2 TextPosition, int PreviousGap, bool Centered) {
    TextObject DataText;

    DataText.FontName = GetFont;
    DataText.TextFill = TextFill;

    if (Centered) {
        DataText.TextPosition = (Vector2){
            (SCREEN_WIDTH / 2) - MeasureTextEx(GetFont, TextFill, FontSize, Spacing).x / 2,
            TextPosition.y + PreviousGap
        };
    } else {
        DataText.TextPosition = (Vector2){
            TextPosition.x,
            TextPosition.y + PreviousGap
        };
    }

    DataText.FontData = (Vector2){ FontSize, Spacing };
    return DataText;
}

void ResetInputs(InputBox* WelcomeInputList, Vector2 CountInputs) {
    for (int i = CountInputs.x; i < CountInputs.y; i++) {
        WelcomeInputList[i].Text[0] = '\0';
        WelcomeInputList[i].LongText[0] = '\0';
        WelcomeInputList[i].MaskedPassword[0] = '\0';
        WelcomeInputList[i].IsValid = true;
    }
}

void GetMenuState(MenuState MenuStatus, char* ReturnMenuState, bool WithInfo) {
    (WithInfo) ? strcpy(ReturnMenuState, "Anda sekarang berada dalam menu: ") : strcpy(ReturnMenuState, "");
    
    if (MenuStatus == MENU_MAIN_GOVERNMENT) {
        strcat(ReturnMenuState, "~ Menu Utama Pemerintah ~");
    } else if (MenuStatus == MENU_MAIN_GOVERNMENT_SchoolManagement) {
        strcat(ReturnMenuState, "~ Menu Utama Pemerintah >> [MANAGE]: Data Sekolah ~");
    } else if (MenuStatus == MENU_MAIN_GOVERNMENT_SchoolManagement_ADD) {
        strcat(ReturnMenuState, "~ Menu Utama Pemerintah >> [MANAGE]: TAMBAH Data Sekolah ~");
    } else if (MenuStatus == MENU_MAIN_GOVERNMENT_SchoolManagement_EDIT) {
        strcat(ReturnMenuState, "~ Menu Utama Pemerintah >> [MANAGE]: UBAH Data Sekolah ~");
    } else if (MenuStatus == MENU_MAIN_GOVERNMENT_SchoolManagement_DELETE) {
        strcat(ReturnMenuState, "~ Menu Utama Pemerintah >> [MANAGE]: HAPUS Data Sekolah ~");
    } else if (MenuStatus == MENU_MAIN_GOVERNMENT_BilateralManagement) {
        strcat(ReturnMenuState, "~ Menu Utama Pemerintah >> [MANAGE]: Data Kerja Sama ~");
    } else if (MenuStatus == MENU_MAIN_GOVERNMENT_BilateralManagement_CHOOSING) {
        strcat(ReturnMenuState, "~ Menu Utama Pemerintah >> [MANAGE]: PILIHAN Data Kerja Sama ~");
    } else if (MenuStatus == MENU_MAIN_GOVERNMENT_GetVendorList) {
        strcat(ReturnMenuState, "~ Menu Utama Pemerintah >> [VIEW]: Data Vendor ~");
    } else if (MenuStatus == MENU_MAIN_GOVERNMENT_GetVendorList_DETAILS) {
        strcat(ReturnMenuState, "~ Menu Utama Pemerintah >> [VIEW]: DETAIL/PROFIL Data Vendor ~");
    } else if (MenuStatus == MENU_MAIN_GOVERNMENT_GetBilateralList) {
        strcat(ReturnMenuState, "~ Menu Utama Pemerintah >> [VIEW]: Data Kerja Sama ~");
    } else if (MenuStatus == MENU_MAIN_GOVERNMENT_ConfirmBudgetPlan) {
        strcat(ReturnMenuState, "~ Menu Utama Pemerintah >> [CONFIRM]: RAB Vendor ~");
    } else if (MenuStatus == MENU_MAIN_GOVERNMENT_ConfirmBudgetPlan_CHOOSING) {
        strcat(ReturnMenuState, "~ Menu Utama Pemerintah >> [CONFIRM]: PEMERIKSAAN RAB Vendor ~");
    } 
    
    else if (MenuStatus == MENU_MAIN_VENDOR) {
        strcat(ReturnMenuState, "~ Menu Utama Vendor/Catering ~");
    } else if (MenuStatus == MENU_MAIN_VENDOR_MenusManagement) {
        strcat(ReturnMenuState, "~ Menu Utama Vendor >> [MANAGE]: Menu Program D'Magis Vendor ~");
    } else if (MenuStatus == MENU_MAIN_VENDOR_MenusManagement_ADD) {
        strcat(ReturnMenuState, "~ Menu Utama Vendor >> [MANAGE]: TAMBAH Menu M.S.G. ~");
    } else if (MenuStatus == MENU_MAIN_VENDOR_MenusManagement_EDIT) {
        strcat(ReturnMenuState, "~ Menu Utama Vendor >> [MANAGE]: UBAH Menu M.S.G. ~");
    } else if (MenuStatus == MENU_MAIN_VENDOR_MenusManagement_DELETE) {
        strcat(ReturnMenuState, "~ Menu Utama Vendor >> [MANAGE]: HAPUS Menu M.S.G. ~");
    } else if (MenuStatus == MENU_MAIN_VENDOR_GetAffiliatedSchoolData) {
        strcat(ReturnMenuState, "~ Menu Utama Vendor >> [VIEW]: Afiliasi Sekolah ~");
    } else if (MenuStatus == MENU_MAIN_VENDOR_SubmitBudgetPlan) {
        strcat(ReturnMenuState, "~ Menu Utama Vendor >> [REQUEST]: Pengajuan RAB Menu ~");
    } else if (MenuStatus == MENU_MAIN_VENDOR_GetDailyMenuStatusList) {
        strcat(ReturnMenuState, "~ Menu Utama Vendor >> [VIEW]: Status Menu ~");
    }

    else if (MenuStatus == MENU_SUBMAIN_SortingOptions) {
        strcat(ReturnMenuState, "~ Pengaturan: Mode Pengurutan Data ~");
    }
}

void DrawPreviousMenuButton(void) {
    if (GoingBack) return;
    
    Vector2 ButtonSize = {200, 60};
    Rectangle PreviousMenuButton = {40, 40, ButtonSize.x, ButtonSize.y};
    Color ButtonColor, TextColor;
    const char *ButtonText = "[<<] Kembali";

    Vector2 Mouse = GetMousePosition();
    if (CheckCollisionPointRec(Mouse, PreviousMenuButton)) {
        HoverTransition[1022] = CustomLerp(HoverTransition[1022], 1.0f, 0.1f);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            NextMenu = PreviousMenu;
            PreviousMenu = CurrentMenu;

            PlaySound(ButtonClickSFX);
            Transitioning = true;
            ScreenFade = 1.0f;
            GoingBack = true;
        }
    } else HoverTransition[1022] = CustomLerp(HoverTransition[1022], 0.0f, 0.1f);

    ButtonColor = (HoverTransition[1022] > 0.5f) ? YELLOW : GRAY;
    TextColor = (HoverTransition[1022] > 0.5f) ? BLACK : WHITE;
    float scale = 1.0f + HoverTransition[1022] * 0.1f;
                
    float newWidth = PreviousMenuButton.width * scale;
    float newHeight = PreviousMenuButton.height * scale;
    float newX = PreviousMenuButton.x - (newWidth - PreviousMenuButton.width) / 2;
    float newY = PreviousMenuButton.y - (newHeight - PreviousMenuButton.height) / 2;
    
    DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, ButtonColor);
    
    Vector2 textWidth = MeasureTextEx(GIFont, ButtonText, FONT_P * scale, SPACING_WIDER);
    int textX = newX + (newWidth / 2) - (textWidth.x / 2);
    int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
    DrawTextEx(GIFont, ButtonText, (Vector2){textX, textY}, FONT_P * scale, SPACING_WIDER, TextColor);
}

void DrawSignOutButton(void) {
    if (GoingBack) return;
    
    Vector2 ButtonSize = {200, 60};
    Rectangle SignOutButton = {40, 40, ButtonSize.x, ButtonSize.y};
    Color ButtonColor, TextColor;
    const char *ButtonText = "[!] Sign Out";

    Vector2 Mouse = GetMousePosition();
    if (CheckCollisionPointRec(Mouse, SignOutButton)) {
        HoverTransition[1023] = CustomLerp(HoverTransition[1023], 1.0f, 0.1f);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            NextMenu = MENU_WELCOME;
            PreviousMenu = MENU_WELCOME;

            memset(&User, 0, sizeof(VendorLoggedInData));

            PlaySound(ButtonClickSFX);
            Transitioning = true;
            ScreenFade = 1.0f;
            GoingBack = true;
        }
    } else HoverTransition[1023] = CustomLerp(HoverTransition[1023], 0.0f, 0.1f);

    ButtonColor = (HoverTransition[1023] > 0.5f) ? RED : LIGHTGRAY;
    TextColor = (HoverTransition[1023] > 0.5f) ? WHITE : BLACK;
    float scale = 1.0f + HoverTransition[1023] * 0.1f;
                
    float newWidth = SignOutButton.width * scale;
    float newHeight = SignOutButton.height * scale;
    float newX = SignOutButton.x - (newWidth - SignOutButton.width) / 2;
    float newY = SignOutButton.y - (newHeight - SignOutButton.height) / 2;
    
    DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, ButtonColor);
    
    Vector2 textWidth = MeasureTextEx(GIFont, ButtonText, FONT_P * scale, SPACING_WIDER);
    int textX = newX + (newWidth / 2) - (textWidth.x / 2);
    int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
    DrawTextEx(GIFont, ButtonText, (Vector2){textX, textY}, FONT_P * scale, SPACING_WIDER, TextColor);
}

void DrawManageViewBorder(int AvailableData) {
    int Gap = 60;
    float Thickness = 2.0f;

    Rectangle Border = {
        .x = 60, .y = 240,
        .width = SCREEN_WIDTH - (Gap * 2),
        .height = 720
    };

    DrawRectangleLinesEx(Border, Thickness, WHITE);
    for (int i = AvailableData; i > 0; i--) {
        DrawLineEx((Vector2){Border.x, Border.y + ((Border.height / 5) * i)}, (Vector2){Border.width + Border.x, Border.y + ((Border.height / 5) * i)}, Thickness, WHITE);
    }
}

void DrawMenuProfileCard(int VendorID, int MenuIndex, float CardOffsetInX, bool ComingFromBPRequest, bool ComingFromBPConfirm, bool ComingFromGovernment) {
    Color CardColor = { 0 }, CardBoxColor = { 0 };
    Vector2 CardSize = {1440, 720};
    Vector2 CardPosition = {(SCREEN_WIDTH / 2) - (CardSize.x / 2) - (int)CardOffsetInX, (SCREEN_HEIGHT / 2) - (CardSize.y / 2)};
    Rectangle CardProperties = {
        CardPosition.x, CardPosition.y, CardSize.x, CardSize.y
    };

    // // //
    Vector2 Mouse = GetMousePosition();
    // // //

    bool IsMenuAvailable = (
        OriginalVendors[VendorID].Menus[MenuIndex].Carbohydrate[0] != 0 ||
        OriginalVendors[VendorID].Menus[MenuIndex].Protein[0] != 0 ||
        OriginalVendors[VendorID].Menus[MenuIndex].Vegetables[0] != 0 ||
        OriginalVendors[VendorID].Menus[MenuIndex].Fruits[0] != 0 ||
        OriginalVendors[VendorID].Menus[MenuIndex].Milk[0] != 0
    );

    if (IsMenuAvailable) {
        if (MenuIndex == 0)         { CardColor = Fade(MAROON, 0.3f);       CardBoxColor = RED;     }
        else if (MenuIndex == 1)    { CardColor = Fade(ORANGE, 0.3f);       CardBoxColor = YELLOW;  }
        else if (MenuIndex == 2)    { CardColor = Fade(DARKGREEN, 0.3f);    CardBoxColor = GREEN;   }
        else if (MenuIndex == 3)    { CardColor = Fade(DARKBLUE, 0.3f);     CardBoxColor = SKYBLUE; }
        else if (MenuIndex == 4)    { CardColor = Fade(DARKPURPLE, 0.3f);   CardBoxColor = PURPLE;  }
    } else {
        CardColor = Fade(GRAY, 0.3f); CardBoxColor = LIGHTGRAY;
    }
    for (int i = 0; i < SCHOOL_DAYS; i++) {
        if (i == MenuIndex) { DrawCircle((SCREEN_WIDTH / 2) - 150 + (60 * (i + 1)), SCREEN_HEIGHT - 40, 6.0f, WHITE); }
        else                { DrawCircle((SCREEN_WIDTH / 2) - 150 + (60 * (i + 1)), SCREEN_HEIGHT - 40, 6.0f,
                ((OriginalVendors[VendorID].BudgetPlanConfirmation[i]) == 0) ? GRAY :
                ((OriginalVendors[VendorID].BudgetPlanConfirmation[i]) == 1) ? ColorBrightness(YELLOW, 0.4f) :
                ((OriginalVendors[VendorID].BudgetPlanConfirmation[i]) == 2) ? ColorBrightness(GREEN, 0.4f) :
                ((OriginalVendors[VendorID].BudgetPlanConfirmation[i]) == -1) ? ColorBrightness(RED, 0.4f) : GRAY
            );
        }
    }

    const char *MenuDays[SCHOOL_DAYS] = {"Menu M.S.G. di hari: SENIN", "Menu M.S.G. di hari: SELASA", "Menu M.S.G. di hari: RABU", "Menu M.S.G. di hari: KAMIS", "Menu M.S.G. di hari: JUM'AT"};
    const char *MenuType = "Jenis Menu Makan: 4 SEHAT 5 SEMPURNA";
    const char *EachMenuPlaceholder[5] = {"Menu Pokok (Karbohidrat):", "Menu Lauk-pauk (Protein):", "Menu Serat dan Mineral (Sayur-sayuran):", "Menu Serat dan Mineral (Buah-buahan):", "Sempurna (Susu):"};
    const char *MenuRequestStatus[] = {
        "Kartu menu berikut belum terdaftarkan. Tambahkan sekarang?",
        "Kartu menu berikut telah terdata, dan Anda masih dapat mengubah\nisi menu selagi pengajuan menu terkait belum disetujui oleh\nPemerintah.",
        "Kartu menu berikut telah disetujui oleh Pemerintah.\nAnda tidak dapat mengubah isi menu ini lagi, kecuali Anda telah\nmenghapusnya terlebih dahulu.",

        "Menu M.S.G. terkait BELUM diajukan kepada Pemerintah. Ajukan sekarang?\nCATATAN: Proses ini TIDAK DAPAT dibatalkan dari pihak Anda!",
        "Menu M.S.G. terkait SUDAH diajukan kepada Pemerintah.\nAnda MASIH dapat mengubah isi menu tersebut walaupun masih dalam PROSES pengajuan.",
        "Pemerintah telah MENERIMA menu harian M.S.G. Anda berikut ini dan akan segera\ndialokasikan kepada Sekolah yang telah ter-afiliasi dengan Anda...",
        "Pemerintah telah MENOLAK menu harian M.S.G. Anda berikut ini.\nAnda dipersilakan untuk mengubah isi menu tersebut dan mengajukannya kembali\nsetelahnya...",
        "Anda belum MENAMBAHKAN Menu M.S.G. Anda pada hari yang bersangkutan.\nPastikan Anda telah menambahkan menu baru berikut sebelum dilakukan proses pengajuan RAB menu Anda kepada Pemerintah!",

        "Apakah Anda ingin memproses lebih lanjut untuk Menu\nM.S.G. terkait dari Vendor berikut?\nCATATAN: Proses ini akan menyebabkan efek PERMANEN!",
        "Anda telah MENERIMA menu harian M.S.G. terkait dari\nVendor yang bersangkutan. Anda masih dapat MENOLAK\npengajuan menu ini apabila diperlukan.",
        "Anda telah MENOLAK menu harian M.S.G. terkait dari\nVendor yang bersangkutan. Anda masih dapat MENERIMA\npengajuan menu ini apabila diperlukan.",
        "Anda belum dapat MENERIMA maupun MENOLAK pengajuan menu harian M.S.G. terkait, dikarenakan dari pihak Vendor yang\nbersangkutan belum meminta pengajuan menu berikut ataupun karena belum dibuatkan.",
    };
    const char *PreparePaginationGuide = "Tekan [<] atau [>] di keyboard laptop/PC Anda untuk bernavigasi...";

    char CorrespondedVendorText[64] = { 0 }, ExplainStatusText[64] = { 0 };
    char CorrespondedVendorText_ANSWER[32] = { 0 }, ExplainStatusText_ANSWER[32] = { 0 };
    strcpy(CorrespondedVendorText, "[INFO] Kepemilikan menu M.S.G berikut adalah dari:");
    snprintf(CorrespondedVendorText_ANSWER, sizeof(CorrespondedVendorText_ANSWER), "%s", OriginalVendors[VendorID].Username);
    strcpy(ExplainStatusText, "[INFO] Status pengajuan menu M.S.G. berikut adalah:");
    snprintf(ExplainStatusText_ANSWER, sizeof(ExplainStatusText_ANSWER), "%s", 
        (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == 0) ? "[-] BELUM DIAJUKAN" :
        (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == 1) ? "[?] SEDANG DITINJAU" :
        (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == 2) ? "[OK] PENGAJUAN DITERIMA" :
        (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == -1) ? "[!] PENGAJUAN DITOLAK" : ""
    );

    TextObject CorrespondedVendor = CreateTextObject(GIFont, CorrespondedVendorText, FONT_P, SPACING_WIDER, (Vector2){CardPosition.x + 30, (CardPosition.y + CardSize.y) - 90}, 0, false);
    TextObject CorrespondedVendor_ANSWER = CreateTextObject(GIFont, CorrespondedVendorText_ANSWER, FONT_P, SPACING_WIDER, (Vector2){(CardPosition.x + CardSize.x) - MeasureTextEx(GIFont, CorrespondedVendorText_ANSWER, FONT_P, SPACING_WIDER).x - 30, (CardPosition.y + CardSize.y) - 90}, 0, false);
    TextObject ExplainStatus = CreateTextObject(GIFont, ExplainStatusText, FONT_P, SPACING_WIDER, (Vector2){CorrespondedVendor.TextPosition.x, CorrespondedVendor.TextPosition.y + 30}, 0, false);
    TextObject ExplainStatus_ANSWER = CreateTextObject(GIFont, ExplainStatusText_ANSWER, FONT_P, SPACING_WIDER, (Vector2){(CardPosition.x + CardSize.x) - MeasureTextEx(GIFont, ExplainStatusText_ANSWER, FONT_P, SPACING_WIDER).x - 30, ExplainStatus.TextPosition.y}, 0, false);
    
    TextObject TO_MenuDays[SCHOOL_DAYS] = {
        CreateTextObject(GIFont, MenuDays[0], FONT_H1, SPACING_WIDER, (Vector2){CardPosition.x + 30, CardPosition.y + 30}, 0, false),
        CreateTextObject(GIFont, MenuDays[1], FONT_H1, SPACING_WIDER, (Vector2){CardPosition.x + 30, CardPosition.y + 30}, 0, false),
        CreateTextObject(GIFont, MenuDays[2], FONT_H1, SPACING_WIDER, (Vector2){CardPosition.x + 30, CardPosition.y + 30}, 0, false),
        CreateTextObject(GIFont, MenuDays[3], FONT_H1, SPACING_WIDER, (Vector2){CardPosition.x + 30, CardPosition.y + 30}, 0, false),
        CreateTextObject(GIFont, MenuDays[4], FONT_H1, SPACING_WIDER, (Vector2){CardPosition.x + 30, CardPosition.y + 30}, 0, false),
    };
    TextObject TO_MenuType = CreateTextObject(GIFont, MenuType, FONT_H2, SPACING_WIDER, (Vector2){TO_MenuDays[0].TextPosition.x, TO_MenuDays[0].TextPosition.y + MeasureTextEx(GIFont, MenuType, FONT_H2, SPACING_WIDER).y}, 20, false);
    TextObject TO_EachMenuPlaceholder[5] = {
        CreateTextObject(GIFont, EachMenuPlaceholder[0], FONT_H3, SPACING_WIDER, (Vector2){CardPosition.x + 60, CardPosition.y + TO_MenuType.TextPosition.y}, 0, false),
        CreateTextObject(GIFont, EachMenuPlaceholder[1], FONT_H3, SPACING_WIDER, (Vector2){CardPosition.x + 60, TO_EachMenuPlaceholder[0].TextPosition.y}, 45, false),
        CreateTextObject(GIFont, EachMenuPlaceholder[2], FONT_H3, SPACING_WIDER, (Vector2){CardPosition.x + 60, TO_EachMenuPlaceholder[1].TextPosition.y}, 45, false),
        CreateTextObject(GIFont, EachMenuPlaceholder[3], FONT_H3, SPACING_WIDER, (Vector2){CardPosition.x + 60, TO_EachMenuPlaceholder[2].TextPosition.y}, 45, false),
        CreateTextObject(GIFont, EachMenuPlaceholder[4], FONT_H3, SPACING_WIDER, (Vector2){CardPosition.x + 60, TO_EachMenuPlaceholder[3].TextPosition.y}, 45, false),
    };
    TextObject PaginationGuide = CreateTextObject(GIFont, PreparePaginationGuide, FONT_H5, SPACING_WIDER, (Vector2){0, CardPosition.y - 30}, 0, true);

    DrawRectangleRec(CardProperties, CardColor);
    DrawRectangleLinesEx(CardProperties, 4.0f, CardBoxColor);

    DrawTextEx(GIFont, TO_MenuDays[MenuIndex].TextFill, TO_MenuDays[MenuIndex].TextPosition, TO_MenuDays[MenuIndex].FontData.x, TO_MenuDays[MenuIndex].FontData.y, WHITE);
    DrawTextEx(GIFont, TO_MenuType.TextFill, TO_MenuType.TextPosition, TO_MenuType.FontData.x, TO_MenuType.FontData.y, LIGHTGRAY);
    for (int i = 0; i < 5; i++) {
        DrawTextEx(GIFont, TO_EachMenuPlaceholder[i].TextFill, TO_EachMenuPlaceholder[i].TextPosition, TO_EachMenuPlaceholder[i].FontData.x, TO_EachMenuPlaceholder[i].FontData.y, LIGHTGRAY);
    }
    
    Rectangle MENUS_Buttons[6] = {
        {(CardPosition.x + CardSize.x - 300), (CardPosition.y + CardSize.y + 20), 300, 60},
        
        MENUS_Buttons[0],
        {(CardPosition.x + CardSize.x - 300) - MENUS_Buttons[0].width - 20, (CardPosition.y + CardSize.y + 20), MENUS_Buttons[0].width, MENUS_Buttons[0].height},
        
        MENUS_Buttons[0],
        
        MENUS_Buttons[0],
        {(CardPosition.x + CardSize.x - 300) - MENUS_Buttons[0].width - 20, (CardPosition.y + CardSize.y + 20), MENUS_Buttons[0].width, MENUS_Buttons[0].height},
    };
    // Only when the menu is coming from the BP request...
    MENUS_Buttons[3].x -= 60;
    MENUS_Buttons[3].width += 60;
    // Only when the menu is coming from the BP request...
    // Only when the menu is coming from the BP confirm...
    MENUS_Buttons[4].x -= 60;
    MENUS_Buttons[4].width += 60;
    MENUS_Buttons[5].x -= 120;
    MENUS_Buttons[5].width += 60;
    // Only when the menu is coming from the BP confirm...

    Color MENUS_ButtonColor[6], MENUS_TextColor[6];
    const char *MENUS_ButtonTexts[] = {
        "[+] Tambah Menu...",
        "[?] Ubah Menu...", "[-] Hapus Menu...",
        "[!] Ajukan Menu M.S.G.",
        "[V] Terima Pengajuan...", "[X] Tolak Pengajuan..."
    };

    DrawLineEx((Vector2){CardPosition.x + 30, TO_MenuType.TextPosition.y + 110}, (Vector2){(CardPosition.x + CardSize.x) - 30, TO_MenuType.TextPosition.y + 110}, 2.0f, WHITE);
    DrawLineEx((Vector2){CardPosition.x + 30, CorrespondedVendor.TextPosition.y - 75}, (Vector2){(CardPosition.x + CardSize.x) - 30, CorrespondedVendor.TextPosition.y - 75}, 2.0f, WHITE);

    if (VendorID != -1) {
        if (!ComingFromGovernment) {
            /*
                Feature: [+] Add Menu...
                Feature: [+] Add Menu...
                Feature: [+] Add Menu...
            */
            if (!IsMenuAvailable && !ComingFromBPRequest && !ComingFromBPConfirm) {
                if (CheckCollisionPointRec(Mouse, MENUS_Buttons[0])) {
                    HoverTransition[891] = CustomLerp(HoverTransition[891], 1.0f, 0.1f);

                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        if (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == 0) {
                            VendorMenuIndex = MenuIndex;

                            NextMenu = MENU_MAIN_VENDOR_MenusManagement_ADD;
                            PreviousMenu = CurrentMenu;
                        }

                        PlaySound(ButtonClickSFX);
                        Transitioning = true;
                        ScreenFade = 1.0f;
                        GoingBack = true;
                    }
                } else HoverTransition[891] = CustomLerp(HoverTransition[891], 0.0f, 0.1f);
                
                MENUS_ButtonColor[0] = (HoverTransition[891] > 0.5f) ? GREEN : DARKGREEN;
                MENUS_TextColor[0] = (HoverTransition[891] > 0.5f) ? BLACK : WHITE;

                float scale = 1.0f + HoverTransition[891] * 0.1f;
                float newWidth = MENUS_Buttons[0].width * scale;
                float newHeight = MENUS_Buttons[0].height * scale;
                float newX = MENUS_Buttons[0].x - (newWidth - MENUS_Buttons[0].width) / 2;
                float newY = MENUS_Buttons[0].y - (newHeight - MENUS_Buttons[0].height) / 2;
                
                DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, MENUS_ButtonColor[0]);

                Vector2 textWidth = MeasureTextEx(GIFont, MENUS_ButtonTexts[0], (FONT_H4 * scale), SPACING_WIDER);
                int textX = newX + (newWidth / 2) - (textWidth.x / 2);
                int textY = newY + (newHeight / 2) - (FONT_H4 * scale / 2);
                DrawTextEx(GIFont, MENUS_ButtonTexts[0], (Vector2){textX, textY}, (FONT_H4 * scale), SPACING_WIDER, MENUS_TextColor[0]);
            }
            
            /*
                Feature: [?] Change Menu..., [-] Remove Menu...
                Feature: [?] Change Menu..., [-] Remove Menu...
                Feature: [?] Change Menu..., [-] Remove Menu...
            */
            else if (IsMenuAvailable && !ComingFromBPRequest && !ComingFromBPConfirm) {
                if (CheckCollisionPointRec(Mouse, MENUS_Buttons[1])) {
                    HoverTransition[892] = CustomLerp(HoverTransition[892], 1.0f, 0.1f);

                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        if (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] != 2) {
                            VendorMenuIndex = MenuIndex;
                            strcpy(MenusManagementInputBox_EDIT[0].Text, OriginalVendors[VendorID].Menus[MenuIndex].Carbohydrate);
                            strcpy(MenusManagementInputBox_EDIT[1].Text, OriginalVendors[VendorID].Menus[MenuIndex].Protein);
                            strcpy(MenusManagementInputBox_EDIT[2].Text, OriginalVendors[VendorID].Menus[MenuIndex].Vegetables);
                            strcpy(MenusManagementInputBox_EDIT[3].Text, OriginalVendors[VendorID].Menus[MenuIndex].Fruits);
                            strcpy(MenusManagementInputBox_EDIT[4].Text, OriginalVendors[VendorID].Menus[MenuIndex].Milk);
                            
                            NextMenu = MENU_MAIN_VENDOR_MenusManagement_EDIT;
                            PreviousMenu = CurrentMenu;
                        }

                        PlaySound(ButtonClickSFX);
                        Transitioning = true;
                        ScreenFade = 1.0f;
                        GoingBack = true;
                    }
                } else HoverTransition[892] = CustomLerp(HoverTransition[892], 0.0f, 0.1f);
                
                if (CheckCollisionPointRec(Mouse, MENUS_Buttons[2])) {
                    HoverTransition[893] = CustomLerp(HoverTransition[893], 1.0f, 0.1f);

                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        VendorMenuIndex = MenuIndex;
                        strcpy(MenusManagementInputBox_DELETE[0].Text, OriginalVendors[VendorID].Menus[MenuIndex].Carbohydrate);
                        strcpy(MenusManagementInputBox_DELETE[1].Text, OriginalVendors[VendorID].Menus[MenuIndex].Protein);
                        strcpy(MenusManagementInputBox_DELETE[2].Text, OriginalVendors[VendorID].Menus[MenuIndex].Vegetables);
                        strcpy(MenusManagementInputBox_DELETE[3].Text, OriginalVendors[VendorID].Menus[MenuIndex].Fruits);
                        strcpy(MenusManagementInputBox_DELETE[4].Text, OriginalVendors[VendorID].Menus[MenuIndex].Milk);

                        NextMenu = MENU_MAIN_VENDOR_MenusManagement_DELETE;
                        PreviousMenu = CurrentMenu;

                        PlaySound(ButtonClickSFX);
                        Transitioning = true;
                        ScreenFade = 1.0f;
                        GoingBack = true;
                    }
                } else HoverTransition[893] = CustomLerp(HoverTransition[893], 0.0f, 0.1f);

                // Check if that current menu are both not from requested and confirmation act before...
                // Double check that if already being confirmed by the government, cannot edit the menu again if it's not deleted already...
                if (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == 2) {
                    MENUS_ButtonColor[1] = (HoverTransition[892] > 0.5f) ? LIGHTGRAY : GRAY;
                    MENUS_TextColor[1] = (HoverTransition[892] > 0.5f) ? BLACK : WHITE;
                } else {
                    MENUS_ButtonColor[1] = (HoverTransition[892] > 0.5f) ? YELLOW : ORANGE;
                    MENUS_TextColor[1] = (HoverTransition[892] > 0.5f) ? BLACK : WHITE;
                }
                MENUS_ButtonColor[2] = (HoverTransition[893] > 0.5f) ? PINK : MAROON;
                MENUS_TextColor[2] = (HoverTransition[893] > 0.5f) ? BLACK : WHITE;
                
                // // //
                float scale = 1.0f + HoverTransition[892] * 0.1f;
                float newWidth = MENUS_Buttons[1].width * scale;
                float newHeight = MENUS_Buttons[1].height * scale;
                float newX = MENUS_Buttons[1].x - (newWidth - MENUS_Buttons[1].width) / 2;
                float newY = MENUS_Buttons[1].y - (newHeight - MENUS_Buttons[1].height) / 2;
                
                DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, MENUS_ButtonColor[1]);

                Vector2 textWidth = MeasureTextEx(GIFont, MENUS_ButtonTexts[1], (FONT_H4 * scale), SPACING_WIDER);
                int textX = newX + (newWidth / 2) - (textWidth.x / 2);
                int textY = newY + (newHeight / 2) - (FONT_H4 * scale / 2);
                DrawTextEx(GIFont, MENUS_ButtonTexts[1], (Vector2){textX, textY}, (FONT_H4 * scale), SPACING_WIDER, MENUS_TextColor[1]);
                // // //
                
                // // //
                scale = 1.0f + HoverTransition[893] * 0.1f;
                newWidth = MENUS_Buttons[2].width * scale;
                newHeight = MENUS_Buttons[2].height * scale;
                newX = MENUS_Buttons[2].x - (newWidth - MENUS_Buttons[2].width) / 2;
                newY = MENUS_Buttons[2].y - (newHeight - MENUS_Buttons[2].height) / 2;
                
                DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, MENUS_ButtonColor[2]);

                textWidth = MeasureTextEx(GIFont, MENUS_ButtonTexts[2], (FONT_H4 * scale), SPACING_WIDER);
                textX = newX + (newWidth / 2) - (textWidth.x / 2);
                textY = newY + (newHeight / 2) - (FONT_H4 * scale / 2);
                DrawTextEx(GIFont, MENUS_ButtonTexts[2], (Vector2){textX, textY}, (FONT_H4 * scale), SPACING_WIDER, MENUS_TextColor[2]);
                // // //
            }
            
            /*
                Feature: [!] Request Menu...
                Feature: [!] Request Menu...
                Feature: [!] Request Menu...
            */
            else if (IsMenuAvailable && ComingFromBPRequest && !ComingFromBPConfirm) {
                if (CheckCollisionPointRec(Mouse, MENUS_Buttons[3])) {
                    HoverTransition[894] = CustomLerp(HoverTransition[894], 1.0f, 0.1f);

                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        if (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == -1 || OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == 0) {
                            ReadUserAccount(Vendors);
                            OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] = 1;
                            WriteUserAccount(OriginalVendors);
                            SyncVendorsFromOriginalVendorData(Vendors, OriginalVendors);
                        }

                        PlaySound(ButtonClickSFX);
                        Transitioning = true;
                        ScreenFade = 1.0f;
                        GoingBack = true;
                    }
                } else HoverTransition[894] = CustomLerp(HoverTransition[894], 0.0f, 0.1f);
                
                // Check if that current menu is already being requested for confirmation...
                if (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == 1 || OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == 2) {
                    MENUS_ButtonColor[3] = (HoverTransition[894] > 0.5f) ? LIGHTGRAY : GRAY;
                    MENUS_TextColor[3] = (HoverTransition[894] > 0.5f) ? BLACK : WHITE;
                } else {
                    MENUS_ButtonColor[3] = (HoverTransition[894] > 0.5f) ? SKYBLUE : DARKBLUE;
                    MENUS_TextColor[3] = (HoverTransition[894] > 0.5f) ? BLACK : WHITE;
                }

                float scale = 1.0f + HoverTransition[894] * 0.1f;
                float newWidth = MENUS_Buttons[3].width * scale;
                float newHeight = MENUS_Buttons[3].height * scale;
                float newX = MENUS_Buttons[3].x - (newWidth - MENUS_Buttons[3].width) / 2;
                float newY = MENUS_Buttons[3].y - (newHeight - MENUS_Buttons[3].height) / 2;
                
                DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, MENUS_ButtonColor[3]);

                Vector2 textWidth = MeasureTextEx(GIFont, MENUS_ButtonTexts[3], (FONT_H4 * scale), SPACING_WIDER);
                int textX = newX + (newWidth / 2) - (textWidth.x / 2);
                int textY = newY + (newHeight / 2) - (FONT_H4 * scale / 2);
                DrawTextEx(GIFont, MENUS_ButtonTexts[3], (Vector2){textX, textY}, (FONT_H4 * scale), SPACING_WIDER, MENUS_TextColor[3]);
            }

            /*
                Feature: [V] Accept Menu Request..., [X] Reject Menu Request...
                Feature: [V] Accept Menu Request..., [X] Reject Menu Request...
                Feature: [V] Accept Menu Request..., [X] Reject Menu Request...
            */
            else if (IsMenuAvailable && !ComingFromBPRequest && ComingFromBPConfirm) {
                if (CheckCollisionPointRec(Mouse, MENUS_Buttons[4])) {
                    HoverTransition[895] = CustomLerp(HoverTransition[895], 1.0f, 0.1f);

                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        if (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == 1) {
                            ReadUserAccount(Vendors);
                            OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] = 2;
                            WriteUserAccount(OriginalVendors);
                            SyncVendorsFromOriginalVendorData(Vendors, OriginalVendors);
                        }

                        PlaySound(ButtonClickSFX);
                        Transitioning = true;
                        ScreenFade = 1.0f;
                        GoingBack = true;
                    }
                } else HoverTransition[895] = CustomLerp(HoverTransition[895], 0.0f, 0.1f);
                
                if (CheckCollisionPointRec(Mouse, MENUS_Buttons[5])) {
                    HoverTransition[896] = CustomLerp(HoverTransition[896], 1.0f, 0.1f);

                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        if (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == 1) {
                            ReadUserAccount(Vendors);
                            OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] = -1;
                            WriteUserAccount(OriginalVendors);
                            SyncVendorsFromOriginalVendorData(Vendors, OriginalVendors);
                        }

                        PlaySound(ButtonClickSFX);
                        Transitioning = true;
                        ScreenFade = 1.0f;
                        GoingBack = true;
                    }
                } else HoverTransition[896] = CustomLerp(HoverTransition[896], 0.0f, 0.1f);

                // Check if that current menu is already being confirmed by the government...
                if (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] != 1) {
                    MENUS_ButtonColor[4] = (HoverTransition[895] > 0.5f) ? LIGHTGRAY : GRAY;
                    MENUS_TextColor[4] = (HoverTransition[895] > 0.5f) ? BLACK : WHITE;
                    MENUS_ButtonColor[5] = (HoverTransition[896] > 0.5f) ? LIGHTGRAY : GRAY;
                    MENUS_TextColor[5] = (HoverTransition[896] > 0.5f) ? BLACK : WHITE;
                } else {
                    MENUS_ButtonColor[4] = (HoverTransition[895] > 0.5f) ? GREEN : DARKGREEN;
                    MENUS_TextColor[4] = (HoverTransition[895] > 0.5f) ? BLACK : WHITE;
                    MENUS_ButtonColor[5] = (HoverTransition[896] > 0.5f) ? PINK : MAROON;
                    MENUS_TextColor[5] = (HoverTransition[896] > 0.5f) ? BLACK : WHITE;
                }
                
                // // //
                float scale = 1.0f + HoverTransition[895] * 0.1f;
                float newWidth = MENUS_Buttons[4].width * scale;
                float newHeight = MENUS_Buttons[4].height * scale;
                float newX = MENUS_Buttons[4].x - (newWidth - MENUS_Buttons[4].width) / 2;
                float newY = MENUS_Buttons[4].y - (newHeight - MENUS_Buttons[4].height) / 2;
                
                DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, MENUS_ButtonColor[4]);

                Vector2 textWidth = MeasureTextEx(GIFont, MENUS_ButtonTexts[4], (FONT_H4 * scale), SPACING_WIDER);
                int textX = newX + (newWidth / 2) - (textWidth.x / 2);
                int textY = newY + (newHeight / 2) - (FONT_H4 * scale / 2);
                DrawTextEx(GIFont, MENUS_ButtonTexts[4], (Vector2){textX, textY}, (FONT_H4 * scale), SPACING_WIDER, MENUS_TextColor[4]);
                // // //
                
                // // //
                scale = 1.0f + HoverTransition[896] * 0.1f;
                newWidth = MENUS_Buttons[5].width * scale;
                newHeight = MENUS_Buttons[5].height * scale;
                newX = MENUS_Buttons[5].x - (newWidth - MENUS_Buttons[5].width) / 2;
                newY = MENUS_Buttons[5].y - (newHeight - MENUS_Buttons[5].height) / 2;
                
                DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, MENUS_ButtonColor[5]);

                textWidth = MeasureTextEx(GIFont, MENUS_ButtonTexts[5], (FONT_H4 * scale), SPACING_WIDER);
                textX = newX + (newWidth / 2) - (textWidth.x / 2);
                textY = newY + (newHeight / 2) - (FONT_H4 * scale / 2);
                DrawTextEx(GIFont, MENUS_ButtonTexts[5], (Vector2){textX, textY}, (FONT_H4 * scale), SPACING_WIDER, MENUS_TextColor[5]);
                // // //
            }
        }
    }

    TextObject TO_MenuDetails[5] = {
        CreateTextObject(GIFont, OriginalVendors[VendorID].Menus[MenuIndex].Carbohydrate, FONT_H3, SPACING_WIDER, (Vector2){(CardPosition.x + 60) + MeasureTextEx(GIFont, EachMenuPlaceholder[2], FONT_H3, SPACING_WIDER).x + 60, CardPosition.y + TO_MenuType.TextPosition.y}, 0, false),
        CreateTextObject(GIFont, OriginalVendors[VendorID].Menus[MenuIndex].Protein, FONT_H3, SPACING_WIDER, (Vector2){(CardPosition.x + 60) + MeasureTextEx(GIFont, EachMenuPlaceholder[2], FONT_H3, SPACING_WIDER).x + 60, TO_EachMenuPlaceholder[0].TextPosition.y}, 45, false),
        CreateTextObject(GIFont, OriginalVendors[VendorID].Menus[MenuIndex].Vegetables, FONT_H3, SPACING_WIDER, (Vector2){(CardPosition.x + 60) + MeasureTextEx(GIFont, EachMenuPlaceholder[2], FONT_H3, SPACING_WIDER).x + 60, TO_EachMenuPlaceholder[1].TextPosition.y}, 45, false),
        CreateTextObject(GIFont, OriginalVendors[VendorID].Menus[MenuIndex].Fruits, FONT_H3, SPACING_WIDER, (Vector2){(CardPosition.x + 60) + MeasureTextEx(GIFont, EachMenuPlaceholder[2], FONT_H3, SPACING_WIDER).x + 60, TO_EachMenuPlaceholder[2].TextPosition.y}, 45, false),
        CreateTextObject(GIFont, OriginalVendors[VendorID].Menus[MenuIndex].Milk, FONT_H3, SPACING_WIDER, (Vector2){(CardPosition.x + 60) + MeasureTextEx(GIFont, EachMenuPlaceholder[2], FONT_H3, SPACING_WIDER).x + 60, TO_EachMenuPlaceholder[3].TextPosition.y}, 45, false),
    };
    for (int i = 0; i < 5; i++) {
        DrawTextEx(GIFont, TO_MenuDetails[i].TextFill, TO_MenuDetails[i].TextPosition, TO_MenuDetails[i].FontData.x, TO_MenuDetails[i].FontData.y, WHITE);
    }

    TextObject TO_MenuRequestStatus[12] = {
        CreateTextObject(GIFont, MenuRequestStatus[0], FONT_H5, SPACING_WIDER, (Vector2){CardPosition.x, CardPosition.y + CardSize.y + 20}, 0, false),
        CreateTextObject(GIFont, MenuRequestStatus[1], FONT_H5, SPACING_WIDER, (Vector2){CardPosition.x, CardPosition.y + CardSize.y + 20}, 0, false),
        CreateTextObject(GIFont, MenuRequestStatus[2], FONT_H5, SPACING_WIDER, (Vector2){CardPosition.x, CardPosition.y + CardSize.y + 20}, 0, false),
        
        CreateTextObject(GIFont, MenuRequestStatus[3], FONT_H5, SPACING_WIDER, (Vector2){CardPosition.x, CardPosition.y + CardSize.y + 20}, 0, false),
        CreateTextObject(GIFont, MenuRequestStatus[4], FONT_H5, SPACING_WIDER, (Vector2){CardPosition.x, CardPosition.y + CardSize.y + 20}, 0, false),
        CreateTextObject(GIFont, MenuRequestStatus[5], FONT_H5, SPACING_WIDER, (Vector2){CardPosition.x, CardPosition.y + CardSize.y + 20}, 0, false),
        CreateTextObject(GIFont, MenuRequestStatus[6], FONT_H5, SPACING_WIDER, (Vector2){CardPosition.x, CardPosition.y + CardSize.y + 20}, 0, false),
        CreateTextObject(GIFont, MenuRequestStatus[7], FONT_H5, SPACING_WIDER, (Vector2){CardPosition.x, CardPosition.y + CardSize.y + 20}, 0, false),

        CreateTextObject(GIFont, MenuRequestStatus[8], FONT_H5, SPACING_WIDER, (Vector2){CardPosition.x, CardPosition.y + CardSize.y + 20}, 0, false),
        CreateTextObject(GIFont, MenuRequestStatus[9], FONT_H5, SPACING_WIDER, (Vector2){CardPosition.x, CardPosition.y + CardSize.y + 20}, 0, false),
        CreateTextObject(GIFont, MenuRequestStatus[10], FONT_H5, SPACING_WIDER, (Vector2){CardPosition.x, CardPosition.y + CardSize.y + 20}, 0, false),
        CreateTextObject(GIFont, MenuRequestStatus[11], FONT_H5, SPACING_WIDER, (Vector2){CardPosition.x, CardPosition.y + CardSize.y + 20}, 0, false),
        
    };

    if (!ComingFromGovernment) {
        if (ComingFromBPRequest) {
            if (IsMenuAvailable) {
                if (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == 0) {
                    DrawTextEx(GIFont, TO_MenuRequestStatus[0 + 3].TextFill, TO_MenuRequestStatus[0 + 3].TextPosition, TO_MenuRequestStatus[0 + 3].FontData.x, TO_MenuRequestStatus[0 + 3].FontData.y, YELLOW);
                } else if (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == 1) {
                    DrawTextEx(GIFont, TO_MenuRequestStatus[1 + 3].TextFill, TO_MenuRequestStatus[1 + 3].TextPosition, TO_MenuRequestStatus[1 + 3].FontData.x, TO_MenuRequestStatus[1 + 3].FontData.y, GREEN);
                } else if (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == 2) {
                    DrawTextEx(GIFont, TO_MenuRequestStatus[2 + 3].TextFill, TO_MenuRequestStatus[2 + 3].TextPosition, TO_MenuRequestStatus[2 + 3].FontData.x, TO_MenuRequestStatus[2 + 3].FontData.y, GREEN);
                } else if (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == -1) {
                    DrawTextEx(GIFont, TO_MenuRequestStatus[3 + 3].TextFill, TO_MenuRequestStatus[3 + 3].TextPosition, TO_MenuRequestStatus[3 + 3].FontData.x, TO_MenuRequestStatus[3 + 3].FontData.y, RED);
                }
            } else {
                DrawTextEx(GIFont, TO_MenuRequestStatus[4 + 3].TextFill, TO_MenuRequestStatus[4 + 3].TextPosition, TO_MenuRequestStatus[4 + 3].FontData.x, TO_MenuRequestStatus[4 + 3].FontData.y, GRAY);
            }
        } else if (ComingFromBPConfirm) {
            if (IsMenuAvailable) {
                if (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == 1) {
                    DrawTextEx(GIFont, TO_MenuRequestStatus[5 + 3].TextFill, TO_MenuRequestStatus[5 + 3].TextPosition, TO_MenuRequestStatus[5 + 3].FontData.x, TO_MenuRequestStatus[5 + 3].FontData.y, YELLOW);
                } else if (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == 2) {
                    DrawTextEx(GIFont, TO_MenuRequestStatus[6 + 3].TextFill, TO_MenuRequestStatus[6 + 3].TextPosition, TO_MenuRequestStatus[6 + 3].FontData.x, TO_MenuRequestStatus[6 + 3].FontData.y, GREEN);
                } else if (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == -1) {
                    DrawTextEx(GIFont, TO_MenuRequestStatus[7 + 3].TextFill, TO_MenuRequestStatus[7 + 3].TextPosition, TO_MenuRequestStatus[7 + 3].FontData.x, TO_MenuRequestStatus[7 + 3].FontData.y, RED);
                }
            } else {
                DrawTextEx(GIFont, TO_MenuRequestStatus[8 + 3].TextFill, TO_MenuRequestStatus[8 + 3].TextPosition, TO_MenuRequestStatus[8 + 3].FontData.x, TO_MenuRequestStatus[8 + 3].FontData.y, GRAY);
            }
        } else {
            if (IsMenuAvailable) {
                if (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == 0 || OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == 1 || OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == -1) {
                    DrawTextEx(GIFont, TO_MenuRequestStatus[1].TextFill, TO_MenuRequestStatus[1].TextPosition, TO_MenuRequestStatus[1].FontData.x, TO_MenuRequestStatus[1].FontData.y, YELLOW);
                } else if (OriginalVendors[VendorID].BudgetPlanConfirmation[MenuIndex] == 2) {
                    DrawTextEx(GIFont, TO_MenuRequestStatus[2].TextFill, TO_MenuRequestStatus[2].TextPosition, TO_MenuRequestStatus[2].FontData.x, TO_MenuRequestStatus[2].FontData.y, GREEN);
                }
            } else {
                DrawTextEx(GIFont, TO_MenuRequestStatus[0].TextFill, TO_MenuRequestStatus[0].TextPosition, TO_MenuRequestStatus[0].FontData.x, TO_MenuRequestStatus[0].FontData.y, SKYBLUE);
            }
        }
    }

    DrawTextEx(GIFont, CorrespondedVendor.TextFill, CorrespondedVendor.TextPosition, CorrespondedVendor.FontData.x, CorrespondedVendor.FontData.y, LIGHTGRAY);
    DrawTextEx(GIFont, CorrespondedVendor_ANSWER.TextFill, CorrespondedVendor_ANSWER.TextPosition, CorrespondedVendor_ANSWER.FontData.x, CorrespondedVendor_ANSWER.FontData.y, WHITE);
    DrawTextEx(GIFont, ExplainStatus.TextFill, ExplainStatus.TextPosition, ExplainStatus.FontData.x, ExplainStatus.FontData.y, LIGHTGRAY);
    if (strstr(ExplainStatus_ANSWER.TextFill, "[-]") != NULL) {
        DrawTextEx(GIFont, ExplainStatus_ANSWER.TextFill, ExplainStatus_ANSWER.TextPosition, ExplainStatus_ANSWER.FontData.x, ExplainStatus_ANSWER.FontData.y, BlendWithWhite(GRAY, 0.6f));
    } else if (strstr(ExplainStatus_ANSWER.TextFill, "[?]") != NULL) {
        DrawTextEx(GIFont, ExplainStatus_ANSWER.TextFill, ExplainStatus_ANSWER.TextPosition, ExplainStatus_ANSWER.FontData.x, ExplainStatus_ANSWER.FontData.y, BlendWithWhite(YELLOW, 0.6f));
    } else if (strstr(ExplainStatus_ANSWER.TextFill, "[OK]") != NULL) {
        DrawTextEx(GIFont, ExplainStatus_ANSWER.TextFill, ExplainStatus_ANSWER.TextPosition, ExplainStatus_ANSWER.FontData.x, ExplainStatus_ANSWER.FontData.y, BlendWithWhite(GREEN, 0.6f));
    } else if (strstr(ExplainStatus_ANSWER.TextFill, "[!]") != NULL) {
        DrawTextEx(GIFont, ExplainStatus_ANSWER.TextFill, ExplainStatus_ANSWER.TextPosition, ExplainStatus_ANSWER.FontData.x, ExplainStatus_ANSWER.FontData.y, BlendWithWhite(RED, 0.6f));
    }
    DrawTextEx(GIFont, PaginationGuide.TextFill, PaginationGuide.TextPosition, PaginationGuide.FontData.x, PaginationGuide.FontData.y, PINK);
}

void MenuSubMain_SortingOptions(void) {
    int SortingButtonsInitialPosition = 160, SortingButtonsGap = 60;
    Vector2 SortingButtonsSize = (Vector2){680, 240};
    char ReturnMenuState[256] = { 0 };

    DrawPreviousMenuButton();

    GetMenuState(CurrentMenu, ReturnMenuState, false);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H2, SPACING_WIDER, (Vector2){0, 60}, 0, true);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);
    
    Rectangle A_TO_Z_SortingButton = {(SCREEN_WIDTH / 2) - SortingButtonsSize.x - (SortingButtonsGap / 2), SortingButtonsInitialPosition, SortingButtonsSize.x, SortingButtonsSize.y};
    Rectangle Z_TO_A_SortingButton = {A_TO_Z_SortingButton.x + SortingButtonsSize.x + SortingButtonsGap, A_TO_Z_SortingButton.y, SortingButtonsSize.x, SortingButtonsSize.y};
    Rectangle LOW_TO_HIGH_SortingButton = {(SCREEN_WIDTH / 2) - SortingButtonsSize.x - (SortingButtonsGap / 2), (Z_TO_A_SortingButton.y + Z_TO_A_SortingButton.height) + SortingButtonsGap, SortingButtonsSize.x, SortingButtonsSize.y};
    Rectangle HIGH_TO_LOW_SortingButton = {LOW_TO_HIGH_SortingButton.x + SortingButtonsSize.x + SortingButtonsGap, LOW_TO_HIGH_SortingButton.y, SortingButtonsSize.x, SortingButtonsSize.y};
    Rectangle NORMAL_SortingMode = {(SCREEN_WIDTH / 2) - (SortingButtonsSize.x / 2), (HIGH_TO_LOW_SortingButton.y + HIGH_TO_LOW_SortingButton.height) + SortingButtonsGap, SortingButtonsSize.x, SortingButtonsSize.y};

    Rectangle Buttons[5] = {A_TO_Z_SortingButton, Z_TO_A_SortingButton, LOW_TO_HIGH_SortingButton, HIGH_TO_LOW_SortingButton, NORMAL_SortingMode};
    Color ButtonColor[5] = { 0 }, TextColor[5] = { 0 };
    const char *ButtonTexts[] = {
        "~ [Aa  ...  Zz]: Teks, dari A ke Z ~",
        "~ [Zz  ...  Aa]: Teks, dari Z ke A ~",
        "~ [MIN ... MAX]: Numerik, dari 0 ke 9,999 ~",
        "~ [MAX ... MIN]: Numerik, dari 9,999 ke 0 ~",
        "~ [NORMAL]: Bentuk Data Orisinal ~",
        "Urutkan data Sekolah atau Vendor/Catering\nsecara alfabetika, dari huruf A ke huruf Z\n\n\n(ascending order, non-case sensitive).",
        "Urutkan data Sekolah atau Vendor/Catering\nsecara alfabetika, dari huruf Z ke huruf A\n\n\n(descending order, non-case sensitive).",
        "Urutkan data Sekolah atau Vendor/Catering\nsecara numerik, dari yang TERKECIL ke yang\nTERBESAR; kuantitasnya dari 0 ke 9,999\n\n(ascending order).",
        "Urutkan data Sekolah atau Vendor/Catering\nsecara numerik, dari yang TERBESAR ke yang\nTERKECIL; kuantitasnya dari 9,999 ke 0\n\n(descending order).",
        "Normalisasikan sistematika pengurutan data\nkembali dalam keadaan sedia kala.\n\n\n(remove the sorting status).",
    };

    const char *SortingTitleMessage = "Mode Pengurutan Data:";
    const char *SortingModeMessage[5] = {
        "[Aa  ...  Zz]: Teks, dari A ke Z",
        "[Zz  ...  Aa]: Teks, dari Z ke A",
        "[MIN ... MAX]: Numerik, dari 0 ke 9,999",
        "[MAX ... MIN]: Numerik, dari 9,999 ke 0",
        "[NORMAL]: Bentuk Data Orisinal",
    };

    TextObject TO_SortingTitleMessage_LEFT = CreateTextObject(GIFont, SortingTitleMessage, FONT_H3, SPACING_WIDER, (Vector2){Buttons[4].x - (MeasureTextEx(GIFont, SortingTitleMessage, FONT_H3, SPACING_WIDER).x / 2) - (Buttons[4].width / 2) + 30, Buttons[4].y + (Buttons[4].height / 4)}, 0, false);
    TextObject TO_SortingTitleMessage_RIGHT = CreateTextObject(GIFont, SortingTitleMessage, FONT_H3, SPACING_WIDER, (Vector2){SCREEN_WIDTH - TO_SortingTitleMessage_LEFT.TextPosition.x - (MeasureTextEx(GIFont, SortingTitleMessage, FONT_H3, SPACING_WIDER).x), Buttons[4].y + (Buttons[4].height / 4)}, 0, false);
    
    TextObject TO_SortingModeMessage_LEFT[5] = {
        CreateTextObject(GIFont, SortingModeMessage[0], FONT_P, SPACING_WIDER, (Vector2){Buttons[4].x - (MeasureTextEx(GIFont, SortingModeMessage[0], FONT_P, SPACING_WIDER).x / 2) - (Buttons[4].width / 2) + 30, Buttons[4].y + (Buttons[4].height / 4) + 45}, 0, false),
        CreateTextObject(GIFont, SortingModeMessage[1], FONT_P, SPACING_WIDER, (Vector2){Buttons[4].x - (MeasureTextEx(GIFont, SortingModeMessage[1], FONT_P, SPACING_WIDER).x / 2) - (Buttons[4].width / 2) + 30, Buttons[4].y + (Buttons[4].height / 4) + 45}, 0, false),
        CreateTextObject(GIFont, SortingModeMessage[2], FONT_P, SPACING_WIDER, (Vector2){Buttons[4].x - (MeasureTextEx(GIFont, SortingModeMessage[2], FONT_P, SPACING_WIDER).x / 2) - (Buttons[4].width / 2) + 30, Buttons[4].y + (Buttons[4].height / 4) + 45}, 0, false),
        CreateTextObject(GIFont, SortingModeMessage[3], FONT_P, SPACING_WIDER, (Vector2){Buttons[4].x - (MeasureTextEx(GIFont, SortingModeMessage[3], FONT_P, SPACING_WIDER).x / 2) - (Buttons[4].width / 2) + 30, Buttons[4].y + (Buttons[4].height / 4) + 45}, 0, false),
        CreateTextObject(GIFont, SortingModeMessage[4], FONT_P, SPACING_WIDER, (Vector2){Buttons[4].x - (MeasureTextEx(GIFont, SortingModeMessage[4], FONT_P, SPACING_WIDER).x / 2) - (Buttons[4].width / 2) + 30, Buttons[4].y + (Buttons[4].height / 4) + 45}, 0, false),
    };
    TextObject TO_SortingModeMessage_RIGHT[5] = {
        CreateTextObject(GIFont, SortingModeMessage[0], FONT_P, SPACING_WIDER, (Vector2){SCREEN_WIDTH - TO_SortingModeMessage_LEFT[0].TextPosition.x - (MeasureTextEx(GIFont, SortingModeMessage[0], FONT_P, SPACING_WIDER).x), Buttons[4].y + (Buttons[4].height / 4) + 45}, 0, false),
        CreateTextObject(GIFont, SortingModeMessage[1], FONT_P, SPACING_WIDER, (Vector2){SCREEN_WIDTH - TO_SortingModeMessage_LEFT[1].TextPosition.x - (MeasureTextEx(GIFont, SortingModeMessage[1], FONT_P, SPACING_WIDER).x), Buttons[4].y + (Buttons[4].height / 4) + 45}, 0, false),
        CreateTextObject(GIFont, SortingModeMessage[2], FONT_P, SPACING_WIDER, (Vector2){SCREEN_WIDTH - TO_SortingModeMessage_LEFT[2].TextPosition.x - (MeasureTextEx(GIFont, SortingModeMessage[2], FONT_P, SPACING_WIDER).x), Buttons[4].y + (Buttons[4].height / 4) + 45}, 0, false),
        CreateTextObject(GIFont, SortingModeMessage[3], FONT_P, SPACING_WIDER, (Vector2){SCREEN_WIDTH - TO_SortingModeMessage_LEFT[3].TextPosition.x - (MeasureTextEx(GIFont, SortingModeMessage[3], FONT_P, SPACING_WIDER).x), Buttons[4].y + (Buttons[4].height / 4) + 45}, 0, false),
        CreateTextObject(GIFont, SortingModeMessage[4], FONT_P, SPACING_WIDER, (Vector2){SCREEN_WIDTH - TO_SortingModeMessage_LEFT[4].TextPosition.x - (MeasureTextEx(GIFont, SortingModeMessage[4], FONT_P, SPACING_WIDER).x), Buttons[4].y + (Buttons[4].height / 4) + 45}, 0, false),
    };

    DrawTextEx(GIFont, TO_SortingTitleMessage_LEFT.TextFill, TO_SortingTitleMessage_LEFT.TextPosition, TO_SortingTitleMessage_LEFT.FontData.x, TO_SortingTitleMessage_LEFT.FontData.y, PINK);
    DrawTextEx(GIFont, TO_SortingTitleMessage_RIGHT.TextFill, TO_SortingTitleMessage_RIGHT.TextPosition, TO_SortingTitleMessage_RIGHT.FontData.x, TO_SortingTitleMessage_RIGHT.FontData.y, PINK);
    
    if (SortingStatus == SORT_A_TO_Z) {
        DrawTextEx(GIFont, TO_SortingModeMessage_LEFT[0].TextFill, TO_SortingModeMessage_LEFT[0].TextPosition, TO_SortingModeMessage_LEFT[0].FontData.x, TO_SortingModeMessage_LEFT[0].FontData.y, SKYBLUE);
        DrawTextEx(GIFont, TO_SortingModeMessage_RIGHT[0].TextFill, TO_SortingModeMessage_RIGHT[0].TextPosition, TO_SortingModeMessage_RIGHT[0].FontData.x, TO_SortingModeMessage_RIGHT[0].FontData.y, SKYBLUE);
    } else if (SortingStatus == SORT_Z_TO_A) {
        DrawTextEx(GIFont, TO_SortingModeMessage_LEFT[1].TextFill, TO_SortingModeMessage_LEFT[1].TextPosition, TO_SortingModeMessage_LEFT[1].FontData.x, TO_SortingModeMessage_LEFT[1].FontData.y, SKYBLUE);
        DrawTextEx(GIFont, TO_SortingModeMessage_RIGHT[1].TextFill, TO_SortingModeMessage_RIGHT[1].TextPosition, TO_SortingModeMessage_RIGHT[1].FontData.x, TO_SortingModeMessage_RIGHT[1].FontData.y, SKYBLUE);
    } else if (SortingStatus == SORT_LOW_TO_HIGH) {
        DrawTextEx(GIFont, TO_SortingModeMessage_LEFT[2].TextFill, TO_SortingModeMessage_LEFT[2].TextPosition, TO_SortingModeMessage_LEFT[2].FontData.x, TO_SortingModeMessage_LEFT[2].FontData.y, SKYBLUE);
        DrawTextEx(GIFont, TO_SortingModeMessage_RIGHT[2].TextFill, TO_SortingModeMessage_RIGHT[2].TextPosition, TO_SortingModeMessage_RIGHT[2].FontData.x, TO_SortingModeMessage_RIGHT[2].FontData.y, SKYBLUE);
    } else if (SortingStatus == SORT_HIGH_TO_LOW) {
        DrawTextEx(GIFont, TO_SortingModeMessage_LEFT[3].TextFill, TO_SortingModeMessage_LEFT[3].TextPosition, TO_SortingModeMessage_LEFT[3].FontData.x, TO_SortingModeMessage_LEFT[3].FontData.y, SKYBLUE);
        DrawTextEx(GIFont, TO_SortingModeMessage_RIGHT[3].TextFill, TO_SortingModeMessage_RIGHT[3].TextPosition, TO_SortingModeMessage_RIGHT[3].FontData.x, TO_SortingModeMessage_RIGHT[3].FontData.y, SKYBLUE);
    } else if (SortingStatus == SORT_NORMAL) {
        DrawTextEx(GIFont, TO_SortingModeMessage_LEFT[4].TextFill, TO_SortingModeMessage_LEFT[4].TextPosition, TO_SortingModeMessage_LEFT[4].FontData.x, TO_SortingModeMessage_LEFT[4].FontData.y, SKYBLUE);
        DrawTextEx(GIFont, TO_SortingModeMessage_RIGHT[4].TextFill, TO_SortingModeMessage_RIGHT[4].TextPosition, TO_SortingModeMessage_RIGHT[4].FontData.x, TO_SortingModeMessage_RIGHT[4].FontData.y, SKYBLUE);
    }
    
    Vector2 Mouse = GetMousePosition();
    for (int i = 91; i < 96; i++) {
        if (CheckCollisionPointRec(Mouse, Buttons[i - 91])) {
            HoverTransition[i] = CustomLerp(HoverTransition[i], 1.0f, 0.1f);
            
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (i == 91) SortingStatus = SORT_A_TO_Z;
                if (i == 92) SortingStatus = SORT_Z_TO_A;
                if (i == 93) SortingStatus = SORT_LOW_TO_HIGH;
                if (i == 94) SortingStatus = SORT_HIGH_TO_LOW;
                if (i == 95) SortingStatus = SORT_NORMAL;

                // TraceLog(LOG_INFO, "================================================================");
                // for (int i = 0; i < MAX_ACCOUNT_LIMIT; i++) {
                //     TraceLog(LOG_INFO, "%s\t|\t%s", OriginalSchools[i], Schools[i]);
                // }
                // TraceLog(LOG_INFO, "================================================================");

                NextMenu = PreviousMenu;
                
                PlaySound(ButtonClickSFX);
                Transitioning = true;
                ScreenFade = 1.0f;
            }

        } else HoverTransition[i] = CustomLerp(HoverTransition[i], 0.0f, 0.1f);
        
        if (i == 91) {
            ButtonColor[i - 91] = (HoverTransition[i] > 0.5f) ? PINK : MAROON;
            TextColor[i - 91] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        } else if (i == 92) {
            ButtonColor[i - 91] = (HoverTransition[i] > 0.5f) ? YELLOW : ORANGE;
            TextColor[i - 91] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        } else if (i == 93) {
            ButtonColor[i - 91] = (HoverTransition[i] > 0.5f) ? GREEN : DARKGREEN;
            TextColor[i - 91] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        } else if (i == 94) {
            ButtonColor[i - 91] = (HoverTransition[i] > 0.5f) ? SKYBLUE : DARKBLUE;
            TextColor[i - 91] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        } else if (i == 95) {
            ButtonColor[i - 91] = (HoverTransition[i] > 0.5f) ? PURPLE : DARKPURPLE;
            TextColor[i - 91] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        }

        float scale = 1.0f + HoverTransition[i] * 0.1f;
        
        float newWidth = Buttons[i - 91].width * scale;
        float newHeight = Buttons[i - 91].height * scale;
        float newX = Buttons[i - 91].x - (newWidth - Buttons[i - 91].width) / 2;
        float newY = Buttons[i - 91].y - (newHeight - Buttons[i - 91].height) / 2;
        
        DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, ButtonColor[i - 91]);
        
        if (i == 91 || i == 92) {
            Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[(i - 91)], (FONT_P * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
            DrawTextEx(GIFont, ButtonTexts[(i - 91)], (Vector2){textX, (textY) - (textY / 6) - 35}, (FONT_P * scale), SPACING_WIDER, TextColor[i - 91]);

            textWidth = MeasureTextEx(GIFont, ButtonTexts[(i - 91) + 5], (FONT_H5 * scale), SPACING_WIDER);
            textX = newX + (newWidth / 2) - (textWidth.x / 2);
            textY = newY + (newHeight / 2) - (FONT_H5 * scale / 2);
            DrawTextEx(GIFont, ButtonTexts[(i - 91) + 5], (Vector2){textX, (textY + 45) - (textY / 6) - 15}, (FONT_H5 * scale), SPACING_WIDER, TextColor[i - 91]);
        } else if (i == 93 || i == 94) {
            Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[(i - 91)], (FONT_P * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
            DrawTextEx(GIFont, ButtonTexts[(i - 91)], (Vector2){textX, (textY) - (textY / 6) + SortingButtonsGap - 40}, (FONT_P * scale), SPACING_WIDER, TextColor[i - 91]);

            textWidth = MeasureTextEx(GIFont, ButtonTexts[(i - 91) + 5], (FONT_H5 * scale), SPACING_WIDER);
            textX = newX + (newWidth / 2) - (textWidth.x / 2);
            textY = newY + (newHeight / 2) - (FONT_H5 * scale / 2);
            DrawTextEx(GIFont, ButtonTexts[(i - 91) + 5], (Vector2){textX, (textY + 45) - (textY / 6) + SortingButtonsGap - 25}, (FONT_H5 * scale), SPACING_WIDER, TextColor[i - 91]);
        } else if (i == 95) {
            Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[(i - 91)], (FONT_P * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
            DrawTextEx(GIFont, ButtonTexts[(i - 91)], (Vector2){textX, (textY) - (textY / 6) + (SortingButtonsGap * 2) - 55}, (FONT_P * scale), SPACING_WIDER, TextColor[i - 91]);

            textWidth = MeasureTextEx(GIFont, ButtonTexts[(i - 91) + 5], (FONT_H5 * scale), SPACING_WIDER);
            textX = newX + (newWidth / 2) - (textWidth.x / 2);
            textY = newY + (newHeight / 2) - (FONT_H5 * scale / 2);
            DrawTextEx(GIFont, ButtonTexts[(i - 91) + 5], (Vector2){textX, (textY + 45) - (textY / 6) + (SortingButtonsGap * 2) - 40}, (FONT_H5 * scale), SPACING_WIDER, TextColor[i - 91]);
        }
    }
}

// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------

void GovernmentMainMenu(void) {
    char ReturnMenuState[256] = { 0 }, SetDayTM[256] = { 0 };
    int MainGovernmentButtonsInitialPosition = 320, MainGovernmentButtonsGap = 60;
    Vector2 MainGovernmentButtonsSize[2] = { (Vector2){680, 350}, (Vector2){((MainGovernmentButtonsSize[0].x * 2) - MainGovernmentButtonsGap) / 3, MainGovernmentButtonsSize[0].y / 1.5} };

    if (GetTimeOfDay() == 1)        strcpy(SetDayTM, TEXT_MENU_MAIN_GOVERNMENT_MORNING);
    else if (GetTimeOfDay() == 2)   strcpy(SetDayTM, TEXT_MENU_MAIN_GOVERNMENT_AFTERNOON);
    else if (GetTimeOfDay() == 3)   strcpy(SetDayTM, TEXT_MENU_MAIN_GOVERNMENT_EVENING);
    else if (GetTimeOfDay() == 4)   strcpy(SetDayTM, TEXT_MENU_MAIN_GOVERNMENT_NIGHT);
    GetMenuState(CurrentMenu, ReturnMenuState, true);

    TextObject TitleMessage = CreateTextObject(GIFont, SetDayTM, FONT_H1, SPACING_WIDER, (Vector2){0, 100}, 20, true);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H3, SPACING_WIDER, (Vector2){0, TitleMessage.TextPosition.y}, 60, true);

    Rectangle FEATURE01_ViewVendorsData_Button = {(SCREEN_WIDTH / 2) - MainGovernmentButtonsSize[0].x - (MainGovernmentButtonsGap / 2), MainGovernmentButtonsInitialPosition, MainGovernmentButtonsSize[0].x, MainGovernmentButtonsSize[0].y};
    Rectangle FEATURE02_ManageSchools_Button = {FEATURE01_ViewVendorsData_Button.x + MainGovernmentButtonsSize[0].x + MainGovernmentButtonsGap, FEATURE01_ViewVendorsData_Button.y, MainGovernmentButtonsSize[0].x, MainGovernmentButtonsSize[0].y};
    Rectangle FEATURE03_ViewBilateralEvents_Button = {(SCREEN_WIDTH / 2) - MainGovernmentButtonsSize[0].x - (MainGovernmentButtonsGap / 2), (FEATURE01_ViewVendorsData_Button.y + MainGovernmentButtonsSize[0].y) + MainGovernmentButtonsGap, MainGovernmentButtonsSize[1].x, MainGovernmentButtonsSize[1].y};
    Rectangle FEATURE04_ManageBilateralEvents_Button = {FEATURE03_ViewBilateralEvents_Button.x + MainGovernmentButtonsSize[1].x + MainGovernmentButtonsGap, (FEATURE01_ViewVendorsData_Button.y + MainGovernmentButtonsSize[0].y) + MainGovernmentButtonsGap, MainGovernmentButtonsSize[1].x, MainGovernmentButtonsSize[1].y};
    Rectangle FEATURE05_BudgetPlanConfirmation_Button = {FEATURE04_ManageBilateralEvents_Button.x + MainGovernmentButtonsSize[1].x + MainGovernmentButtonsGap, (FEATURE01_ViewVendorsData_Button.y + MainGovernmentButtonsSize[0].y) + MainGovernmentButtonsGap, MainGovernmentButtonsSize[1].x, MainGovernmentButtonsSize[1].y};
    
    Rectangle Buttons[5] = {FEATURE01_ViewVendorsData_Button, FEATURE02_ManageSchools_Button, FEATURE03_ViewBilateralEvents_Button, FEATURE04_ManageBilateralEvents_Button, FEATURE05_BudgetPlanConfirmation_Button};
    Color ButtonColor[5] = { 0 }, TextColor[5] = { 0 };
    const char *ButtonTexts[] = {
        "~ [MANAGE]: Data Sekolah ~",
        "~ [MANAGE]: Data Kerja Sama ~",
        "~ [VIEW]: Data Vendor ~",
        "~ [CONFIRM]: RAB Vendor ~",
        "~ [VIEW]: Data Kerja Sama ~",
        "Tambah, ubah, dan hapus data sekolah terkait\nsebelum diajukan sebagai pihak bilateral\ndengan Vendor.\n\nData sekolah hanya berisikan nama sekolah terkait\ndan banyaknya siswa yang bersekolah di sekolah\ntersebut.",
        "Tambah, ubah, dan hapus data kerja sama pihak\nVendor dengan sekolah terkait (yang telah\ndidaftarkan sebelumnya).\n\nPemutusan kerja sama dilakukan secara sepihak\ndan masing-masing SATU (1) porsinya dari kedua\nbelah pihak.",
        "Lihat semua data Vendor yang\nterdaftar dalam database D'Magis,\nnamun tidak diperkenankan untuk\nmengubah data pribadi Vendor\nterkait.",
        "Berikan konfirmasi terkait\npersetujuan yang diminta dari\nVendor yang bersangkutan.",
        "Lihat semua data kerja sama\nyang dijalani oleh Vendor dengan\nsekolah yang bersangkutan.",
    };
    
    // Always updates the concurrent data of Vendors and Schools whenever changes or no changes are detected.
    // Again, just for safety reasons...
    // ReadUserAccount(Vendors);
    // ReadSchoolsData(Schools);
    // for (int i = 0; i < MAX_ACCOUNT_LIMIT; i++) {
    //     for (int j = 0; j < MAX_ACCOUNT_LIMIT; j++) {
    //         if (OriginalVendors[i].SaveSchoolDataIndex == j) {
    //             if (strcmp(OriginalVendors[i].AffiliatedSchoolData.Name, OriginalSchools[j].Name) != 0) { strcpy(OriginalVendors[i].AffiliatedSchoolData.Name, OriginalSchools[j].Name); }
    //             if (strcmp(OriginalVendors[i].AffiliatedSchoolData.Students, OriginalSchools[j].Students) != 0) { strcpy(OriginalVendors[i].AffiliatedSchoolData.Students, OriginalSchools[j].Students); }
    //         }
    //     }
    // } 
    // WriteUserAccount(OriginalVendors);
    // WriteSchoolsData(OriginalSchools);
    // SyncVendorsFromOriginalVendorData(Vendors, OriginalVendors);
    // SyncSchoolsFromOriginalSchoolData(Schools, OriginalSchools);
    // Always updates the concurrent data of Vendors and Schools whenever changes or no changes are detected.
    // Again, just for safety reasons...

    // Always reset the menu profile cards...
    GetTemporaryVendorIndex = -1;
    IsCardTransitioning = false;
    CurrentCardMenuProfile = 0;
    SetCardPositionInX = 0; // Start off-screen left
    TargetX = 0;
    TransitionDirection = 0; // 1 = right, -1 = left
    // Always reset the menu profile cards...

    DrawTextEx(GIFont, TitleMessage.TextFill, TitleMessage.TextPosition, TitleMessage.FontData.x, TitleMessage.FontData.y, WHITE);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);
    
    DrawSignOutButton();
    
    Vector2 Mouse = GetMousePosition();
    for (int i = 11; i < 16; i++) {
        if (CheckCollisionPointRec(Mouse, Buttons[i - 11])) {
            HoverTransition[i] = CustomLerp(HoverTransition[i], 1.0f, 0.1f);
            
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (i == 11) NextMenu = MENU_MAIN_GOVERNMENT_SchoolManagement;
                if (i == 12) NextMenu = MENU_MAIN_GOVERNMENT_BilateralManagement;
                if (i == 13) NextMenu = MENU_MAIN_GOVERNMENT_GetVendorList;
                if (i == 14) NextMenu = MENU_MAIN_GOVERNMENT_ConfirmBudgetPlan;
                if (i == 15) NextMenu = MENU_MAIN_GOVERNMENT_GetBilateralList;
                
                PlaySound(ButtonClickSFX);
                Transitioning = true;
                ScreenFade = 1.0f;
            }

        } else HoverTransition[i] = CustomLerp(HoverTransition[i], 0.0f, 0.1f);
        
        if (i == 11) {
            ButtonColor[i - 11] = (HoverTransition[i] > 0.5f) ? PINK : MAROON;
            TextColor[i - 11] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        } else if (i == 12) {
            ButtonColor[i - 11] = (HoverTransition[i] > 0.5f) ? YELLOW : ORANGE;
            TextColor[i - 11] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        } else if (i == 13) {
            ButtonColor[i - 11] = (HoverTransition[i] > 0.5f) ? GREEN : DARKGREEN;
            TextColor[i - 11] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        } else if (i == 14) {
            ButtonColor[i - 11] = (HoverTransition[i] > 0.5f) ? SKYBLUE : DARKBLUE;
            TextColor[i - 11] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        } else if (i == 15) {
            ButtonColor[i - 11] = (HoverTransition[i] > 0.5f) ? PURPLE : DARKPURPLE;
            TextColor[i - 11] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        }

        float scale = 1.0f + HoverTransition[i] * 0.1f;
        
        float newWidth = Buttons[i - 11].width * scale;
        float newHeight = Buttons[i - 11].height * scale;
        float newX = Buttons[i - 11].x - (newWidth - Buttons[i - 11].width) / 2;
        float newY = Buttons[i - 11].y - (newHeight - Buttons[i - 11].height) / 2;
        
        DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, ButtonColor[i - 11]);
        
        if (i == 11 || i == 12) {
            Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[(i - 11)], (FONT_H3 * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_H3 * scale / 2);
            DrawTextEx(GIFont, ButtonTexts[(i - 11)], (Vector2){textX, (textY - (textY / 4))}, (FONT_H3 * scale), SPACING_WIDER, TextColor[i - 11]);

            textWidth = MeasureTextEx(GIFont, ButtonTexts[(i - 11) + 5], (FONT_H5 * scale), SPACING_WIDER);
            textX = newX + (newWidth / 2) - (textWidth.x / 2);
            textY = newY + (newHeight / 2) - (FONT_H5 * scale / 2);
            DrawTextEx(GIFont, ButtonTexts[(i - 11) + 5], (Vector2){textX, (textY + 75) - (textY / 4)}, (FONT_H5 * scale), SPACING_WIDER, TextColor[i - 11]);
        } else {
            Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[(i - 11)], (FONT_P * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
            DrawTextEx(GIFont, ButtonTexts[(i - 11)], (Vector2){textX, (textY) - (textY / 12)}, (FONT_P * scale), SPACING_WIDER, TextColor[i - 11]);

            textWidth = MeasureTextEx(GIFont, ButtonTexts[(i - 11) + 5], (FONT_H5 * scale), SPACING_WIDER);
            textX = newX + (newWidth / 2) - (textWidth.x / 2);
            textY = newY + (newHeight / 2) - (FONT_H5 * scale / 2);
            DrawTextEx(GIFont, ButtonTexts[(i - 11) + 5], (Vector2){textX, (textY + 45) - (textY / 12)}, (FONT_H5 * scale), SPACING_WIDER, TextColor[i - 11]);
        }
    }
}

void GovernmentManageSchoolsMenu(void) {
    char ReturnMenuState[256] = { 0 };
    int LeftMargin = 40, TopMargin = 20;
    Vector2 EditDeleteButtonSize = {150, 50};
    
    Rectangle ADD_GOTOPAGE_Buttons[2] = {
        {SCREEN_WIDTH - 300, 160, 240, 60}, 
        {160, 160, 120, 60}
    };
    Color ADD_GOTOPAGE_ButtonColor[2], ADD_GOTOPAGE_TextColor[2];
    const char *ADD_GOTOPAGE_ButtonTexts[] = {"[+] Tambah...", "Tuju!"};
    
    const char *SortingSettingButtonText = "[@] Urutkan...";
    Rectangle SortingSettingButton = {ADD_GOTOPAGE_Buttons[0].x, ADD_GOTOPAGE_Buttons[0].y - ADD_GOTOPAGE_Buttons[0].height - TopMargin, ADD_GOTOPAGE_Buttons[0].width, ADD_GOTOPAGE_Buttons[0].height};
    Color SortingSettingButtonColor, SortingSettingTextColor;

    Rectangle EditDeleteButtons[10] = {
        {ADD_GOTOPAGE_Buttons[0].x + 72, (ADD_GOTOPAGE_Buttons[0].y + 80) + 18, EditDeleteButtonSize.x, EditDeleteButtonSize.y},
        {ADD_GOTOPAGE_Buttons[0].x + 72, EditDeleteButtons[0].y + 60, EditDeleteButtonSize.x, EditDeleteButtonSize.y},

        {ADD_GOTOPAGE_Buttons[0].x + 72, (EditDeleteButtons[0].y + 144), EditDeleteButtonSize.x, EditDeleteButtonSize.y},
        {ADD_GOTOPAGE_Buttons[0].x + 72, EditDeleteButtons[2].y + 60, EditDeleteButtonSize.x, EditDeleteButtonSize.y},
        
        {ADD_GOTOPAGE_Buttons[0].x + 72, (EditDeleteButtons[2].y + 144), EditDeleteButtonSize.x, EditDeleteButtonSize.y},
        {ADD_GOTOPAGE_Buttons[0].x + 72, EditDeleteButtons[4].y + 60, EditDeleteButtonSize.x, EditDeleteButtonSize.y},

        {ADD_GOTOPAGE_Buttons[0].x + 72, (EditDeleteButtons[4].y + 144), EditDeleteButtonSize.x, EditDeleteButtonSize.y},
        {ADD_GOTOPAGE_Buttons[0].x + 72, EditDeleteButtons[6].y + 60, EditDeleteButtonSize.x, EditDeleteButtonSize.y},
        
        {ADD_GOTOPAGE_Buttons[0].x + 72, (EditDeleteButtons[6].y + 144), EditDeleteButtonSize.x, EditDeleteButtonSize.y},
        {ADD_GOTOPAGE_Buttons[0].x + 72, EditDeleteButtons[8].y + 60, EditDeleteButtonSize.x, EditDeleteButtonSize.y}
    };
    Color EditDeleteButtonColor[10], EditDeleteTextColor[10];
    const char *EditDeleteButtonTexts[] = {"[?] Ubah...", "[-] Hapus..."};

    PreviousMenu = MENU_MAIN_GOVERNMENT;
    DrawPreviousMenuButton();

    GetMenuState(CurrentMenu, ReturnMenuState, false);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H2, SPACING_WIDER, (Vector2){0, 60}, 0, true);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

    const char *DataTexts[] = {
        "Mohon maaf, saat ini belum ada data Sekolah yang terdaftarkan oleh Anda.",
        "Anda dipersilakan untuk menambahkan data Sekolah baru (beserta banyak siswa-nya) pada menu `[+] Tambah...` di atas.",
        "Terima kasih atas kerja samanya, Pemerintah setempat..."
    };
    TextObject NoSchoolsMessage[3] = {
        CreateTextObject(GIFont, DataTexts[0], FONT_H5, SPACING_WIDER, (Vector2){0, (SCREEN_HEIGHT / 2) - 30}, 0, true),
        CreateTextObject(GIFont, DataTexts[1], FONT_H5, SPACING_WIDER, (Vector2){0, NoSchoolsMessage[0].TextPosition.y}, 40, true),
        CreateTextObject(GIFont, DataTexts[2], FONT_H3, SPACING_WIDER, (Vector2){0, NoSchoolsMessage[1].TextPosition.y}, 80, true),
    };

    char SAD_No[16] = { 0 }, SAD_GeneratedSecondLineMeasurement[32] = { 0 };
    TextObject SAD_Numberings[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_FirstLine[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_SchoolName[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_SecondLine[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_AffiliatedVendor[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_ThirdLine[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_ManyStudents[MAX_ACCOUNT_LIMIT] = { 0 };
    
    GoToPageInputBox[0].Box = (Rectangle){60, ADD_GOTOPAGE_Buttons[1].y, 80, 60};

    char ShowRegisteredSchoolsText[128] = { 0 };
    const char *PrepareGoToPageGuide = "Pergi ke halaman:";
    const char *PreparePaginationGuide = "Tekan [<] atau [>] di keyboard laptop/PC Anda untuk bernavigasi...";
    char PreparePaginationTextInfo[64] = { 0 };
    snprintf(ShowRegisteredSchoolsText, sizeof(ShowRegisteredSchoolsText), "Sebanyak `%d dari %d` Sekolah telah terdata...", ReadSchools, MAX_ACCOUNT_LIMIT);
    TextObject ShowRegisteredSchools = CreateTextObject(GIFont, ShowRegisteredSchoolsText, FONT_P, SPACING_WIDER, (Vector2){0, 200}, 0, true);
    snprintf(PreparePaginationTextInfo, sizeof(PreparePaginationTextInfo), "Halaman %d dari %d", (CurrentPages[0] + 1), (ReadSchools % DISPLAY_PER_PAGE != 0) ? ((ReadSchools / DISPLAY_PER_PAGE) + 1) : (ReadSchools / DISPLAY_PER_PAGE));
    TextObject GoToPageGuide = CreateTextObject(GIFont, PrepareGoToPageGuide, FONT_H5, SPACING_WIDER, (Vector2){GoToPageInputBox[0].Box.x, GoToPageInputBox[0].Box.y - (GoToPageInputBox[0].Box.height / 2)}, 0, false);
    TextObject PaginationInfo = CreateTextObject(GIFont, PreparePaginationTextInfo, FONT_P, SPACING_WIDER, (Vector2){SCREEN_WIDTH - 60 - MeasureTextEx(GIFont, PreparePaginationTextInfo, FONT_P, SPACING_WIDER).x, 980}, 0, false);
    TextObject PaginationGuide = CreateTextObject(GIFont, PreparePaginationGuide, FONT_H5, SPACING_WIDER, (Vector2){SCREEN_WIDTH - 60 - MeasureTextEx(GIFont, PreparePaginationGuide, FONT_H5, SPACING_WIDER).x, PaginationInfo.TextPosition.y}, 40, false);

    ReadSchools = ReadSchoolsData(Schools);
    SchoolsOnPage = ReadSchoolsWithPagination(CurrentPages[0], Schools);

    // Resetting the error and success messages first...
    HasClicked[4] = false; HasClicked[5] = false;
    SchoolsManagementInputBox_ADD[0].Text[0] = '\0'; SchoolsManagementInputBox_ADD[1].Text[0] = '\0';
    SchoolsManagementInputBox_ADD_AllValid = true;
    SchoolsManagementInputBox_EDIT_AllValid = true;
    SchoolDataAlreadyExists = false;
    // Resetting the error and success messages first...
    
    Vector2 Mouse = GetMousePosition();
    for (int i = 0; i < 3; i++) {
        if (i != 2) {
            if (CheckCollisionPointRec(Mouse, ADD_GOTOPAGE_Buttons[i])) {
                HoverTransition[i + 991] = CustomLerp(HoverTransition[i + 991], 1.0f, 0.1f);

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    if (i == 0) {
                        NextMenu = MENU_MAIN_GOVERNMENT_SchoolManagement_ADD;
                        PreviousMenu = CurrentMenu;
                    } if (i == 1 && ReadSchools > 0) {
                        int RequestedPage = atoi(GoToPageInputBox[0].Text);
                        if (RequestedPage < 1)  { RequestedPage = 1;  }
                        if (RequestedPage > 26) { RequestedPage = 26; }

                        int MaxAvailablePage = (ReadSchools + DISPLAY_PER_PAGE - 1) / DISPLAY_PER_PAGE; // Ensures proper rounding
                        if (RequestedPage > MaxAvailablePage) { RequestedPage = MaxAvailablePage; }

                        snprintf(GoToPageInputBox[0].Text, sizeof(GoToPageInputBox[0].Text), "%02d", RequestedPage);
                        CurrentPages[0] = RequestedPage - 1;
                    } if (i == 2) {
                        NextMenu = MENU_SUBMAIN_SortingOptions;
                        PreviousMenu = CurrentMenu;
                    }

                    PlaySound(ButtonClickSFX);
                    Transitioning = true;
                    ScreenFade = 1.0f;
                    GoingBack = true;
                }
            } else HoverTransition[i + 991] = CustomLerp(HoverTransition[i + 991], 0.0f, 0.1f);
        } else {
            if (CheckCollisionPointRec(Mouse, SortingSettingButton)) {
                HoverTransition[i + 991] = CustomLerp(HoverTransition[i + 991], 1.0f, 0.1f);

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    if (i == 2) {
                        NextMenu = MENU_SUBMAIN_SortingOptions;
                        PreviousMenu = CurrentMenu;
                    }

                    PlaySound(ButtonClickSFX);
                    Transitioning = true;
                    ScreenFade = 1.0f;
                    GoingBack = true;
                }
            } else HoverTransition[i + 991] = CustomLerp(HoverTransition[i + 991], 0.0f, 0.1f);
        }
        
        if (i == 0) {
            ADD_GOTOPAGE_ButtonColor[0] = (HoverTransition[i + 991] > 0.5f) ? SKYBLUE : DARKBLUE;
            ADD_GOTOPAGE_TextColor[0] = (HoverTransition[i + 991] > 0.5f) ? BLACK : WHITE;
        } else if (i == 1 && ReadSchools > 0) {
            ADD_GOTOPAGE_ButtonColor[1] = (HoverTransition[i + 991] > 0.5f) ? SKYBLUE : DARKBLUE;
            ADD_GOTOPAGE_TextColor[1] = (HoverTransition[i + 991] > 0.5f) ? BLACK : WHITE;
        } else if (i == 2) {
            SortingSettingButtonColor = (HoverTransition[i + 991] > 0.5f) ? PURPLE : DARKPURPLE;
            SortingSettingTextColor = (HoverTransition[i + 991] > 0.5f) ? BLACK : WHITE;
        }
        
        float scale, newWidth, newHeight, newX, newY;
        if (i >= 0 && i < 2) {
            scale = 1.0f + HoverTransition[i + 991] * 0.1f;
            newWidth = ADD_GOTOPAGE_Buttons[i].width * scale;
            newHeight = ADD_GOTOPAGE_Buttons[i].height * scale;
            newX = ADD_GOTOPAGE_Buttons[i].x - (newWidth - ADD_GOTOPAGE_Buttons[i].width) / 2;
            newY = ADD_GOTOPAGE_Buttons[i].y - (newHeight - ADD_GOTOPAGE_Buttons[i].height) / 2;
        } else if (i == 2) {
            scale = 1.0f + HoverTransition[i + 991] * 0.1f;
            newWidth = SortingSettingButton.width * scale;
            newHeight = SortingSettingButton.height * scale;
            newX = SortingSettingButton.x - (newWidth - SortingSettingButton.width) / 2;
            newY = SortingSettingButton.y - (newHeight - SortingSettingButton.height) / 2;
        }
        
        if (i != 2) DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, ADD_GOTOPAGE_ButtonColor[i]);
        else DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, SortingSettingButtonColor);

        if (i == 0) {
            Vector2 textWidth = MeasureTextEx(GIFont, ADD_GOTOPAGE_ButtonTexts[0], (FONT_H3 * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_H3 * scale / 2);
            DrawTextEx(GIFont, ADD_GOTOPAGE_ButtonTexts[0], (Vector2){textX, textY}, (FONT_H3 * scale), SPACING_WIDER, ADD_GOTOPAGE_TextColor[i]);
        } else if (i == 1 && ReadSchools > 0) {
            Vector2 textWidth = MeasureTextEx(GIFont, ADD_GOTOPAGE_ButtonTexts[1], (FONT_H3 * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_H3 * scale / 2);
            DrawTextEx(GIFont, ADD_GOTOPAGE_ButtonTexts[1], (Vector2){textX, textY}, (FONT_H3 * scale), SPACING_WIDER, ADD_GOTOPAGE_TextColor[i]);
        } else if (i == 2) {
            Vector2 textWidth = MeasureTextEx(GIFont, SortingSettingButtonText, (FONT_H3 * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_H3 * scale / 2);
            DrawTextEx(GIFont, SortingSettingButtonText, (Vector2){textX, textY}, (FONT_H3 * scale), SPACING_WIDER, SortingSettingTextColor);
        }
    }

    if (ReadSchools == 0) {
        // DrawManageViewBorder(0);
        for (int i = 0; i < 3; i++) {
            DrawTextEx(GIFont, NoSchoolsMessage[i].TextFill, NoSchoolsMessage[i].TextPosition, NoSchoolsMessage[i].FontData.x, NoSchoolsMessage[i].FontData.y, (i != 2) ? YELLOW : PINK);
        }
    } else {
        {
            DrawRectangleRec(GoToPageInputBox[0].Box, GRAY);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (CheckCollisionPointRec(Mouse, GoToPageInputBox[0].Box)) { GoToPageInputBox[0].IsActive = true; }
                else { GoToPageInputBox[0].IsActive = false; }
            }

            if (GoToPageInputBox[0].IsActive) {
                int Key = GetCharPressed();
                while (Key >= '0' && Key <= '9') {
                    if (strlen(GoToPageInputBox[0].Text) < 2) {
                        int len = strlen(GoToPageInputBox[0].Text);
                        GoToPageInputBox[0].Text[len] = (char)Key;
                        GoToPageInputBox[0].Text[len + 1] = '\0';
                    } Key = GetCharPressed();
                }

                if (IsKeyPressed(KEY_BACKSPACE) && strlen(GoToPageInputBox[0].Text) > 0) {
                    GoToPageInputBox[0].Text[strlen(GoToPageInputBox[0].Text) - 1] = '\0';
                }
            }

            FrameCounter++; // Increment frame counter for cursor blinking
            Color boxColor = GRAY;    
            if (GoToPageInputBox[0].IsActive) boxColor = SKYBLUE; // Active input
            else if (CheckCollisionPointRec(Mouse, GoToPageInputBox[0].Box)) boxColor = YELLOW; // Hover effect

            DrawRectangleRec(GoToPageInputBox[0].Box, boxColor);
            DrawTextEx(GIFont, GoToPageInputBox[0].Text, (Vector2){GoToPageInputBox[0].Box.x + 5, GoToPageInputBox[0].Box.y + 5}, FONT_H1, SPACING_WIDER, BLACK);

            if (GoToPageInputBox[0].IsActive && (FrameCounter / CURSOR_BLINK_SPEED) % 2 == 0) {
                int textWidth = MeasureTextEx(GIFont, GoToPageInputBox[0].Text, FONT_H1, SPACING_WIDER).x;
                DrawTextEx(GIFont, "|", (Vector2){GoToPageInputBox[0].Box.x + 5 + textWidth, GoToPageInputBox[0].Box.y + 5}, FONT_H1, SPACING_WIDER, BLACK);
            } if (strlen(GoToPageInputBox[0].Text) == 0 && !GoToPageInputBox[0].IsActive) {
                DrawTextEx(GIFont, "01", (Vector2){GoToPageInputBox[0].Box.x + 5, GoToPageInputBox[0].Box.y + 5}, FONT_H1, SPACING_WIDER, Fade(BLACK, 0.6));
            }
        }

        for (int i = 0; i < (SchoolsOnPage * 2); i++) {
            if (CheckCollisionPointRec(Mouse, EditDeleteButtons[i])) {
                HoverTransition[i + 980] = CustomLerp(HoverTransition[i + 980], 1.0f, 0.1f);

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    if (i % 2 == 0) {
                        strcpy(SchoolsManagementInputBox_EDIT[0].Text, Schools[(i / 2) + (CurrentPages[0] * DISPLAY_PER_PAGE)].Name);
                        strcpy(SchoolsManagementInputBox_EDIT[1].Text, Schools[(i / 2) + (CurrentPages[0] * DISPLAY_PER_PAGE)].Students);
                        strcpy(OldSchoolName, SchoolsManagementInputBox_EDIT[0].Text);
                        NextMenu = MENU_MAIN_GOVERNMENT_SchoolManagement_EDIT;
                    } if (i % 2 != 0) {
                        strcpy(SchoolsManagementInputBox_DELETE[0].Text, Schools[(i / 2) + (CurrentPages[0] * DISPLAY_PER_PAGE)].Name);
                        strcpy(SchoolsManagementInputBox_DELETE[1].Text, Schools[(i / 2) + (CurrentPages[0] * DISPLAY_PER_PAGE)].Students);
                        strcpy(SchoolsManagementInputBox_DELETE[2].Text, Schools[(i / 2) + (CurrentPages[0] * DISPLAY_PER_PAGE)].AffiliatedVendor);
                        NextMenu = MENU_MAIN_GOVERNMENT_SchoolManagement_DELETE;
                    }

                    PlaySound(ButtonClickSFX);
                    Transitioning = true;
                    ScreenFade = 1.0f;
                    GoingBack = true;
                }
            } else HoverTransition[i + 980] = CustomLerp(HoverTransition[i + 980], 0.0f, 0.1f);
            
            if (i % 2 == 0) {
                EditDeleteButtonColor[i] = (HoverTransition[i + 980] > 0.5f) ? YELLOW : ORANGE;
                EditDeleteTextColor[i] = (HoverTransition[i + 980] > 0.5f) ? BLACK : WHITE;
            } else {
                EditDeleteButtonColor[i] = (HoverTransition[i + 980] > 0.5f) ? PINK : MAROON;
                EditDeleteTextColor[i] = (HoverTransition[i + 980] > 0.5f) ? BLACK : WHITE;
            }
                
            float scale = 1.0f + HoverTransition[i + 980] * 0.1f;
            float newWidth = EditDeleteButtons[i].width * scale;
            float newHeight = EditDeleteButtons[i].height * scale;
            float newX = EditDeleteButtons[i].x - (newWidth - EditDeleteButtons[i].width) / 2;
            float newY = EditDeleteButtons[i].y - (newHeight - EditDeleteButtons[i].height) / 2;
            
            DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, EditDeleteButtonColor[i]);

            Vector2 textWidth = MeasureTextEx(GIFont, EditDeleteButtonTexts[(i % 2 == 0) ? 0 : 1], (FONT_H5 * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_H5 * scale / 2);
            DrawTextEx(GIFont, EditDeleteButtonTexts[(i % 2 == 0) ? 0 : 1], (Vector2){textX, textY}, (FONT_H5 * scale), SPACING_WIDER, EditDeleteTextColor[i]);
        }

        DrawManageViewBorder((SchoolsOnPage < 5) ? SchoolsOnPage : 4);
        for (int i = 0; i < SchoolsOnPage; i++) {
            snprintf(SAD_No, sizeof(SAD_No), "%03d", (i + 1) + (CurrentPages[0] * DISPLAY_PER_PAGE));
            SAD_Numberings[i].TextFill = SAD_No;
            SAD_Numberings[i].TextPosition = (Vector2){60 + LeftMargin, 240 + (144 * i) + ((TopMargin * 5) / 2) + 3};
            SAD_Numberings[i].FontData = (Vector2){FONT_H2, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_Numberings[i].TextFill, SAD_Numberings[i].TextPosition, SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y, SKYBLUE);

            SAD_FirstLine[i].TextFill = "Nama Sekolah:";
            SAD_FirstLine[i].TextPosition = (Vector2){
                (60 + LeftMargin) + (MeasureTextEx(GIFont, "000", SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y).x + LeftMargin),
                240 + (144 * i) + (TopMargin / 2)
            };
            SAD_FirstLine[i].FontData = (Vector2){FONT_H5, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_FirstLine[i].TextFill, SAD_FirstLine[i].TextPosition, SAD_FirstLine[i].FontData.x, SAD_FirstLine[i].FontData.y, WHITE);

            SAD_SchoolName[i].TextFill = Schools[i].Name;
            SAD_SchoolName[i].TextPosition = (Vector2){
                SAD_FirstLine[i].TextPosition.x,
                SAD_FirstLine[i].TextPosition.y + MeasureTextEx(GIFont, SAD_Numberings[i].TextFill, SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y).y + (TopMargin / 4)
            };
            SAD_SchoolName[i].FontData = (Vector2){(strlen(SAD_SchoolName[i].TextFill) <= 20) ? FONT_H1 : FONT_H2, SPACING_WIDER};
            SAD_SchoolName[i].TextPosition.y -= (SAD_SchoolName[i].FontData.x == FONT_H1) ? 12 : 8;
            DrawTextEx(GIFont, SAD_SchoolName[i].TextFill, SAD_SchoolName[i].TextPosition, SAD_SchoolName[i].FontData.x, SAD_SchoolName[i].FontData.y, PURPLE);

            SAD_ThirdLine[i].TextFill = "Banyak Murid:";
            SAD_ThirdLine[i].TextPosition = (Vector2){
                (60 + LeftMargin) + (MeasureTextEx(GIFont, "000", SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y).x + LeftMargin),
                (240 + (144 * i) + TopMargin) + MeasureTextEx(GIFont, SAD_Numberings[i].TextFill, SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y).y + 48
            };
            SAD_ThirdLine[i].FontData = (Vector2){FONT_H5, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_ThirdLine[i].TextFill, SAD_ThirdLine[i].TextPosition, SAD_ThirdLine[i].FontData.x, SAD_ThirdLine[i].FontData.y, WHITE);

            SAD_ManyStudents[i].TextFill = Schools[i].Students;
            SAD_ManyStudents[i].TextPosition = (Vector2){
                SAD_ThirdLine[i].TextPosition.x + MeasureTextEx(GIFont, SAD_ThirdLine[i].TextFill, SAD_ThirdLine[i].FontData.x, SAD_ThirdLine[i].FontData.y).x + 10,
                SAD_ThirdLine[i].TextPosition.y - 2.38
            };
            SAD_ManyStudents[i].FontData = (Vector2){FONT_H4, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_ManyStudents[i].TextFill, SAD_ManyStudents[i].TextPosition, SAD_ManyStudents[i].FontData.x, SAD_ManyStudents[i].FontData.y, BEIGE);
            
            for (int c = 0; c < MAX_INPUT_LENGTH; c++) { SAD_GeneratedSecondLineMeasurement[c] = 'O'; }
            SAD_SecondLine[i].TextFill = "Afiliasi Vendor (atau Catering):";
            SAD_SecondLine[i].TextPosition = (Vector2){
                (((60 + LeftMargin) + SAD_FirstLine[i].TextPosition.x) * 1.5) + MeasureTextEx(GIFont, SAD_GeneratedSecondLineMeasurement, SAD_FirstLine[i].FontData.x, SAD_FirstLine[i].FontData.y).x,
                240 + (144 * i) + TopMargin
            };
            SAD_SecondLine[i].FontData = (Vector2){FONT_H5, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_SecondLine[i].TextFill, SAD_SecondLine[i].TextPosition, SAD_SecondLine[i].FontData.x, SAD_SecondLine[i].FontData.y, WHITE);
            SAD_GeneratedSecondLineMeasurement[0] = '\0';

            if (strlen(Schools[i].AffiliatedVendor) > 0) { SAD_AffiliatedVendor[i].TextFill = Schools[i].AffiliatedVendor; }
            else { SAD_AffiliatedVendor[i].TextFill = "-"; }
            SAD_AffiliatedVendor[i].TextPosition = (Vector2){
                SAD_SecondLine[i].TextPosition.x,
                SAD_SecondLine[i].TextPosition.y + MeasureTextEx(GIFont, SAD_Numberings[i].TextFill, SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y).y
            };
            SAD_AffiliatedVendor[i].FontData = (Vector2){(strlen(SAD_AffiliatedVendor[i].TextFill) <= 20) ? FONT_H1 : FONT_H2, SPACING_WIDER};
            SAD_AffiliatedVendor[i].TextPosition.y += (SAD_AffiliatedVendor[i].FontData.x == FONT_H1) ? 2 : 8;
            DrawTextEx(GIFont, SAD_AffiliatedVendor[i].TextFill, SAD_AffiliatedVendor[i].TextPosition, SAD_AffiliatedVendor[i].FontData.x, SAD_AffiliatedVendor[i].FontData.y, YELLOW);
        }

        if (IsKeyPressed(KEY_RIGHT) && SchoolsOnPage == DISPLAY_PER_PAGE && (CurrentPages[0] * DISPLAY_PER_PAGE) + SchoolsOnPage < ReadSchools) { CurrentPages[0]++; }
        if (IsKeyPressed(KEY_LEFT) && CurrentPages[0] > 0) { CurrentPages[0]--; }
        
        DrawTextEx(GIFont, ShowRegisteredSchools.TextFill, ShowRegisteredSchools.TextPosition, ShowRegisteredSchools.FontData.x, ShowRegisteredSchools.FontData.y, PINK);
        DrawTextEx(GIFont, GoToPageGuide.TextFill, GoToPageGuide.TextPosition, GoToPageGuide.FontData.x, GoToPageGuide.FontData.y, WHITE);
        DrawTextEx(GIFont, PaginationInfo.TextFill, PaginationInfo.TextPosition, PaginationInfo.FontData.x, PaginationInfo.FontData.y, WHITE);
        DrawTextEx(GIFont, PaginationGuide.TextFill, PaginationGuide.TextPosition, PaginationGuide.FontData.x, PaginationGuide.FontData.y, GREEN);
    }
}

void GovernmentManageSchoolsMenu_ADD(void) {
    char ReturnMenuState[256] = { 0 };
    int ResetSubmitButtonGap = 40, ButtonsGap = 80;
    Vector2 SchoolDataInputBoxSize = {900, 60};
    Vector2 ResetSubmitButtonSize = {(SchoolDataInputBoxSize.x / 2) - (ButtonsGap / 4), 60};

    PreviousMenu = MENU_MAIN_GOVERNMENT_SchoolManagement;
    DrawPreviousMenuButton();

    GetMenuState(CurrentMenu, ReturnMenuState, false);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H2, SPACING_WIDER, (Vector2){0, 60}, 0, true);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

    const char *DataTexts[] = {
        "Silakan untuk menambahkan data Sekolah baru, dengan setiap masukkan yang telah disesuaikan...",
        "Nama Sekolah (cth. SMAN 1 DPS)",
        "Banyak Murid (cth. 343)",
        
        "Rentang: 2 s/d 30 Karakter",
        "Banyak: 10 s/d 9,999 Siswa",
        
        "Sukses! Anda telah berhasil mendaftarkan Sekolah terkait ke dalam data Anda...",

        "Gagal! Terdapat masukkan data Sekolah yang masih belum memenuhi kriteria...",
        "Gagal! Data `Nama Sekolah` yang Anda sertakan telah terdaftar sebelumnya...",

        "Berikut adalah tempat di mana Anda akan mendaftarkan Sekolah baru agar dapat segera diafiliasikan kepada pihak Vendor (atau Catering), nantinya pada\nmenu `[MANAGE]: Data Kerja Sama`. Data yang diminta hanyalah `Nama Sekolah` dan `Banyak Murid`, yang di mana proses pendaftarannya SEKALI setiap\nSUBMIT.\n\nPerlu diperhatikan bahwa maksimum pendataannya HANYA sebesar `128` Sekolah saja, dan SATU data Sekolah yang sudah ada TIDAK DAPAT terdaftarkan\nkembali, kecuali diubah (dalam menu tersendiri) atau dihapus terlebih dahulu!",

        "Mohon maaf, data Sekolah yang tercatat telah memenuhi target pembuatan data Sekolah maksimum yang",
        "ditetapkan oleh program D'Magis ini...",
        "Adapun tujuannya diperketat sedemikian rupa, demi menjaga kestabilan data untuk sementara waktu ini",
        "dibatasi hingga 128 data saja yang mampu disimpan.",
        "Apabila Anda masih berkenan untuk menambahkan data Sekolah baru, mohon untuk menunggu pembaruan dari",
        "program D'Magis ini yang akan dirilis nantinya secara FULL (tuntas) pada selang waktu ke depan...",
        "Terima kasih atas perhatiannya, Pemerintah setempat..."
    };
    const char *ButtonTexts[2] = {"Reset...", "Daftarkan..."};
    
    Rectangle ResetButton = {(SCREEN_WIDTH / 2) - ResetSubmitButtonSize.x - (ResetSubmitButtonGap / 2), SchoolsManagementInputBox_ADD[1].Box.y + ButtonsGap, ResetSubmitButtonSize.x, ResetSubmitButtonSize.y};
    Rectangle SubmitButton = {ResetButton.x + ResetSubmitButtonSize.x + ResetSubmitButtonGap, ResetButton.y, ResetSubmitButtonSize.x, ResetSubmitButtonSize.y};
    Rectangle Buttons[2] = {ResetButton, SubmitButton};
    Color ButtonColor[2] = { 0 }, TextColor[2] = { 0 };

    TextObject InputBoxHelp[2] = {
        CreateTextObject(GIFont, DataTexts[1], (SchoolDataInputBoxSize.y / 2), SPACING_WIDER, (Vector2){SchoolsManagementInputBox_ADD[0].Box.x + 10, SchoolsManagementInputBox_ADD[0].Box.y + 15}, 0, false),
        CreateTextObject(GIFont, DataTexts[2], (SchoolDataInputBoxSize.y / 2), SPACING_WIDER, (Vector2){SchoolsManagementInputBox_ADD[1].Box.x + 10, SchoolsManagementInputBox_ADD[1].Box.y + 15}, 0, false)
    };
    TextObject MaxCharsInfo[2] = {
        CreateTextObject(GIFont, DataTexts[3], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER, (Vector2){SchoolsManagementInputBox_ADD[0].Box.x + SchoolsManagementInputBox_ADD[0].Box.width - (MeasureTextEx(GIFont, DataTexts[3], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER).x) - 15, SchoolsManagementInputBox_ADD[0].Box.y + (SchoolDataInputBoxSize.y / 3)}, 0, false),
        CreateTextObject(GIFont, DataTexts[4], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER, (Vector2){SchoolsManagementInputBox_ADD[1].Box.x + SchoolsManagementInputBox_ADD[1].Box.width - (MeasureTextEx(GIFont, DataTexts[4], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER).x) - 15, SchoolsManagementInputBox_ADD[1].Box.y + (SchoolDataInputBoxSize.y / 3)}, 0, false)
    };

    TextObject AddSchoolMessage = CreateTextObject(GIFont, DataTexts[0], FONT_H3, SPACING_WIDER, (Vector2){0, 240 + 40}, 0, true);
    TextObject SuccessMessage = CreateTextObject(GIFont, DataTexts[5], FONT_H3, SPACING_WIDER, (Vector2){0, AddSchoolMessage.TextPosition.y}, 40, true);
    TextObject FirstErrorMessage = CreateTextObject(GIFont, DataTexts[6], FONT_H3, SPACING_WIDER, (Vector2){0, AddSchoolMessage.TextPosition.y}, 40, true);
    TextObject SecondErrorMessage = CreateTextObject(GIFont, DataTexts[7], FONT_H3, SPACING_WIDER, (Vector2){0, AddSchoolMessage.TextPosition.y}, 40, true);
    TextObject HelpInfo = CreateTextObject(GIFont, DataTexts[8], FONT_H5, SPACING_WIDER, (Vector2){100, ResetButton.y}, 75 + ResetButton.height, false);

    TextObject SchoolDataIsFullMessage[7] = {
        CreateTextObject(GIFont, DataTexts[9], FONT_P, SPACING_WIDER, (Vector2){0, (SCREEN_HEIGHT / 2) - 150}, 0, true),
        CreateTextObject(GIFont, DataTexts[10], FONT_P, SPACING_WIDER, (Vector2){0, SchoolDataIsFullMessage[0].TextPosition.y}, 30, true),
        CreateTextObject(GIFont, DataTexts[11], FONT_P, SPACING_WIDER, (Vector2){0, SchoolDataIsFullMessage[1].TextPosition.y}, 30, true),
        CreateTextObject(GIFont, DataTexts[12], FONT_P, SPACING_WIDER, (Vector2){0, SchoolDataIsFullMessage[2].TextPosition.y}, 30, true),
        CreateTextObject(GIFont, DataTexts[13], FONT_P, SPACING_WIDER, (Vector2){0, SchoolDataIsFullMessage[3].TextPosition.y}, 80 + 30, true),
        CreateTextObject(GIFont, DataTexts[14], FONT_P, SPACING_WIDER, (Vector2){0, SchoolDataIsFullMessage[4].TextPosition.y}, 30, true),
        CreateTextObject(GIFont, DataTexts[15], FONT_P, SPACING_WIDER, (Vector2){0, SchoolDataIsFullMessage[5].TextPosition.y}, 80 + 30, true)
    };
    
    SchoolsManagementInputBox_ADD[0].Box = (Rectangle){(SCREEN_WIDTH / 2) - (SchoolDataInputBoxSize.x / 2), 480, SchoolDataInputBoxSize.x, SchoolDataInputBoxSize.y};
    SchoolsManagementInputBox_ADD[1].Box = (Rectangle){(SCREEN_WIDTH / 2) - (SchoolDataInputBoxSize.x / 2), SchoolsManagementInputBox_ADD[0].Box.y + ButtonsGap, SchoolDataInputBoxSize.x, SchoolDataInputBoxSize.y};

    if (ReadSchoolsData(Schools) == MAX_ACCOUNT_LIMIT) {
        for (int i = 0; i < 7; i++) {
            if (i >= 0 && i < 4) DrawTextEx(GIFont, SchoolDataIsFullMessage[i].TextFill, SchoolDataIsFullMessage[i].TextPosition, SchoolDataIsFullMessage[i].FontData.x, SchoolDataIsFullMessage[i].FontData.y, WHITE);
            else if (i >= 4 && i < 6) DrawTextEx(GIFont, SchoolDataIsFullMessage[i].TextFill, SchoolDataIsFullMessage[i].TextPosition, SchoolDataIsFullMessage[i].FontData.x, SchoolDataIsFullMessage[i].FontData.y, YELLOW);
            else if (i == 6) DrawTextEx(GIFont, SchoolDataIsFullMessage[i].TextFill, SchoolDataIsFullMessage[i].TextPosition, SchoolDataIsFullMessage[i].FontData.x, SchoolDataIsFullMessage[i].FontData.y, GREEN);
        }
        
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            PreviousMenu = MENU_MAIN_GOVERNMENT_SchoolManagement;
            NextMenu = MENU_MAIN_GOVERNMENT_SchoolManagement;
            CurrentMenu = NextMenu;

            PlaySound(ButtonClickSFX);
            SuccessfulWriteUserAccount = false;
            HasClicked[4] = false;
            Transitioning = true;
            ScreenFade = 1.0f;
        }
    } else {
        DrawManageViewBorder(0);

        if (HasClicked[4]) {
            if (!SchoolDataAlreadyExists) {
                if (SchoolsManagementInputBox_ADD_AllValid) {
                    DrawTextEx(GIFont, FirstErrorMessage.TextFill, FirstErrorMessage.TextPosition, FirstErrorMessage.FontData.x, FirstErrorMessage.FontData.y, Fade(BLACK, 0.0f));
                    DrawTextEx(GIFont, SecondErrorMessage.TextFill, SecondErrorMessage.TextPosition, SecondErrorMessage.FontData.x, SecondErrorMessage.FontData.y, Fade(BLACK, 0.0f));
                    DrawTextEx(GIFont, SuccessMessage.TextFill, SuccessMessage.TextPosition, SuccessMessage.FontData.x, SuccessMessage.FontData.y, GREEN);
                } else {
                    DrawTextEx(GIFont, SuccessMessage.TextFill, SuccessMessage.TextPosition, SuccessMessage.FontData.x, SuccessMessage.FontData.y, Fade(BLACK, 0.0f));
                    DrawTextEx(GIFont, SecondErrorMessage.TextFill, SecondErrorMessage.TextPosition, SecondErrorMessage.FontData.x, SecondErrorMessage.FontData.y, Fade(BLACK, 0.0f));
                    DrawTextEx(GIFont, FirstErrorMessage.TextFill, FirstErrorMessage.TextPosition, FirstErrorMessage.FontData.x, FirstErrorMessage.FontData.y, RED);
                }
            } else {
                DrawTextEx(GIFont, FirstErrorMessage.TextFill, FirstErrorMessage.TextPosition, FirstErrorMessage.FontData.x, FirstErrorMessage.FontData.y, Fade(BLACK, 0.0f));
                DrawTextEx(GIFont, SuccessMessage.TextFill, SuccessMessage.TextPosition, SuccessMessage.FontData.x, SuccessMessage.FontData.y, Fade(BLACK, 0.0f));
                DrawTextEx(GIFont, SecondErrorMessage.TextFill, SecondErrorMessage.TextPosition, SecondErrorMessage.FontData.x, SecondErrorMessage.FontData.y, RED);
            }
        }

        DrawTextEx(GIFont, AddSchoolMessage.TextFill, AddSchoolMessage.TextPosition, AddSchoolMessage.FontData.x, AddSchoolMessage.FontData.y, PINK);
        DrawTextEx(GIFont, HelpInfo.TextFill, HelpInfo.TextPosition, HelpInfo.FontData.x, HelpInfo.FontData.y, BEIGE);
        DrawRectangleRec(SchoolsManagementInputBox_ADD[0].Box, GRAY);
        DrawRectangleRec(SchoolsManagementInputBox_ADD[1].Box, GRAY);

        Vector2 Mouse = GetMousePosition();
        for (int i = 0; i < 2; i++) {
            if (CheckCollisionPointRec(Mouse, Buttons[i])) {
                HoverTransition[1006 + i] = CustomLerp(HoverTransition[1006 + i], 1.0f, 0.1f);
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    if (i == 0) ResetInputs(SchoolsManagementInputBox_ADD, (Vector2){0, 2});
                    if (i == 1) {
                        SchoolsManagementInputBox_ADD_AllValid = true;

                        for (int j = 0; j < 2; j++) {
                            if (j == 0) { SchoolsManagementInputBox_ADD[j].IsValid = (strlen(SchoolsManagementInputBox_ADD[j].Text) >= 2); }
                            else if (j == 1) { SchoolsManagementInputBox_ADD[j].IsValid = (atoi(SchoolsManagementInputBox_ADD[j].Text) >= 10 && atoi(SchoolsManagementInputBox_ADD[j].Text) <= 9999); }

                            if (!SchoolsManagementInputBox_ADD[j].IsValid) { SchoolsManagementInputBox_ADD_AllValid = false; SchoolDataAlreadyExists = false; }
                        }

                        ReadSchoolsData(Schools);

                        SchoolsManagementInputBox_ADD_AllValid = false;  // Default to false
                        for (int s = 0; s < MAX_ACCOUNT_LIMIT; s++) {
                            if (strlen(SchoolsManagementInputBox_ADD[0].Text) > 0 && strcmp(OriginalSchools[s].Name, SchoolsManagementInputBox_ADD[0].Text) == 0) {
                                SchoolsManagementInputBox_ADD_AllValid = false;
                                SchoolDataAlreadyExists = true;
                                break;
                            } 

                            if (strlen(OriginalSchools[s].Name) == 0 && strlen(OriginalSchools[s].Students) == 0 && SchoolsManagementInputBox_ADD[0].IsValid && SchoolsManagementInputBox_ADD[1].IsValid) {
                                strcpy(OriginalSchools[s].Name, SchoolsManagementInputBox_ADD[0].Text);
                                strcpy(OriginalSchools[s].Students, SchoolsManagementInputBox_ADD[1].Text);
                                strcpy(OriginalSchools[s].AffiliatedVendor, "");
                                
                                SyncSchoolsFromOriginalSchoolData(Schools, OriginalSchools);
                                WriteSchoolsData(OriginalSchools);
                                
                                SchoolsManagementInputBox_ADD_AllValid = true;
                                SchoolDataAlreadyExists = false;
                                break;
                            }
                        }
                        
                        HasClicked[4] = true;
                    }
                    
                    PlaySound(ButtonClickSFX);
                    Transitioning = true;
                    ScreenFade = 1.0f;
                }
            } else HoverTransition[1006 + i] = CustomLerp(HoverTransition[1006 + i], 0.0f, 0.1f);
            
            if (i == 0) { // Reset button
                ButtonColor[i] = (HoverTransition[1006 + i] > 0.5f) ? RED : LIGHTGRAY;
                TextColor[i] = (HoverTransition[1006 + i] > 0.5f) ? WHITE : BLACK;
            } else if (i == 1) { // Submit button
                ButtonColor[i] = (HoverTransition[1006 + i] > 0.5f) ? GREEN : LIGHTGRAY;
                TextColor[i] = (HoverTransition[1006 + i] > 0.5f) ? WHITE : BLACK;
            }

            float scale = 1.0f + HoverTransition[1006 + i] * 0.1f;
            
            float newWidth = Buttons[i].width * scale;
            float newHeight = Buttons[i].height * scale;
            float newX = Buttons[i].x - (newWidth - Buttons[i].width) / 2;
            float newY = Buttons[i].y - (newHeight - Buttons[i].height) / 2;
            
            DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, ButtonColor[i]);
            
            Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[i], (FONT_P * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
            DrawTextEx(GIFont, ButtonTexts[i], (Vector2){textX, textY}, (FONT_P * scale), SPACING_WIDER, TextColor[i]);
        }
        
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            ActiveBox = -1;  // Reset active box
            for (int i = 0; i < 2; i++) {
                if (CheckCollisionPointRec(Mouse, SchoolsManagementInputBox_ADD[i].Box)) {
                    ActiveBox = i;
                    break;
                }
            }
        }

        if (ActiveBox != -1) {
            int Key = GetCharPressed();
            if (ActiveBox == 0) {
                while (Key > 0) {
                    if (strlen(SchoolsManagementInputBox_ADD[ActiveBox].Text) < MAX_INPUT_LENGTH) {
                        int len = strlen(SchoolsManagementInputBox_ADD[ActiveBox].Text);
                        SchoolsManagementInputBox_ADD[ActiveBox].Text[len] = (char)Key;
                        SchoolsManagementInputBox_ADD[ActiveBox].Text[len + 1] = '\0';
                    } Key = GetCharPressed();
                }
            } else if (ActiveBox == 1) {
                while (Key >= '0' && Key <= '9') {
                    if (strlen(SchoolsManagementInputBox_ADD[ActiveBox].Text) < 4) {
                        int len = strlen(SchoolsManagementInputBox_ADD[ActiveBox].Text);
                        SchoolsManagementInputBox_ADD[ActiveBox].Text[len] = (char)Key;
                        SchoolsManagementInputBox_ADD[ActiveBox].Text[len + 1] = '\0';
                    } Key = GetCharPressed();
                }
            }

            if (IsKeyPressed(KEY_BACKSPACE) && strlen(SchoolsManagementInputBox_ADD[ActiveBox].Text) > 0) {
                SchoolsManagementInputBox_ADD[ActiveBox].Text[strlen(SchoolsManagementInputBox_ADD[ActiveBox].Text) - 1] = '\0';
            } else if (IsKeyDown(KEY_BACKSPACE) && strlen(SchoolsManagementInputBox_ADD[ActiveBox].Text) > 0) {
                CurrentFrameTime += GetFrameTime();
                if (CurrentFrameTime >= 0.5f) {
                    SchoolsManagementInputBox_ADD[ActiveBox].Text[strlen(SchoolsManagementInputBox_ADD[ActiveBox].Text) - 1] = '\0';
                }
            }
            
            if (IsKeyReleased(KEY_BACKSPACE)) CurrentFrameTime = 0;
        }

        FrameCounter++; // Increment frame counter for cursor blinking
        for (int i = 0; i < 2; i++) {
            Color boxColor = GRAY;
            if (!SchoolsManagementInputBox_ADD[i].IsValid && HasClicked[4]) { boxColor = RED; }
            
            if (i == ActiveBox) boxColor = SKYBLUE; // Active input
            else if (CheckCollisionPointRec(Mouse, SchoolsManagementInputBox_ADD[i].Box)) boxColor = YELLOW; // Hover effect

            DrawRectangleRec(SchoolsManagementInputBox_ADD[i].Box, boxColor);
            DrawTextEx(GIFont, SchoolsManagementInputBox_ADD[i].Text, (Vector2){SchoolsManagementInputBox_ADD[i].Box.x + 10, SchoolsManagementInputBox_ADD[i].Box.y + 15}, (SchoolDataInputBoxSize.y / 2), SPACING_WIDER, BLACK);

            if (i == ActiveBox && (FrameCounter / CURSOR_BLINK_SPEED) % 2 == 0) {
                int textWidth = MeasureTextEx(GIFont, SchoolsManagementInputBox_ADD[i].Text, (SchoolDataInputBoxSize.y / 2), SPACING_WIDER).x;
                DrawTextEx(GIFont, "|", (Vector2){SchoolsManagementInputBox_ADD[i].Box.x + 10 + textWidth, SchoolsManagementInputBox_ADD[i].Box.y + 10}, (SchoolDataInputBoxSize.y / 2) + 10, SPACING_WIDER, BLACK);
            }
            
            DrawTextEx(GIFont, MaxCharsInfo[i].TextFill, MaxCharsInfo[i].TextPosition, MaxCharsInfo[i].FontData.x, MaxCharsInfo[i].FontData.y, BLACK);
            if (strlen(SchoolsManagementInputBox_ADD[i].Text) == 0 && i != ActiveBox) {
                DrawTextEx(GIFont, InputBoxHelp[i].TextFill, InputBoxHelp[i].TextPosition, InputBoxHelp[i].FontData.x, InputBoxHelp[i].FontData.y, Fade(BLACK, 0.6));
            }
        }
    }
}

void GovernmentManageSchoolsMenu_EDIT(void) {
    char ReturnMenuState[256] = { 0 };
    int ResetSubmitButtonGap = 40, ButtonsGap = 80;
    Vector2 SchoolDataInputBoxSize = {900, 60};
    Vector2 ResetSubmitButtonSize = {(SchoolDataInputBoxSize.x / 2) - (ButtonsGap / 4), 60};

    PreviousMenu = MENU_MAIN_GOVERNMENT_SchoolManagement;
    DrawPreviousMenuButton();

    GetMenuState(CurrentMenu, ReturnMenuState, false);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H2, SPACING_WIDER, (Vector2){0, 60}, 0, true);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

    const char *DataTexts[] = {
        "Silakan untuk mengubah data Sekolah terkait, dengan setiap masukkan yang telah disesuaikan...",
        "Nama Sekolah (cth. SMAN 1 DPS)",
        "Banyak Murid (cth. 343)",
        
        "Rentang: 2 s/d 30 Karakter",
        "Banyak: 10 s/d 9,999 Siswa",
        
        "Sukses! Anda telah berhasil mengubah data Sekolah terkait dari dalam data Anda...",

        "Gagal! Terdapat masukkan data Sekolah yang masih belum memenuhi kriteria...",
        "Gagal! Data `Nama Sekolah` yang Anda sertakan telah terdaftar sebelumnya...",

        "Berikut adalah tempat di mana Anda akan mendaftarkan Sekolah baru agar dapat segera diafiliasikan kepada pihak Vendor (atau Catering), nantinya pada\nmenu `[MANAGE]: Data Kerja Sama`. Data yang diminta hanyalah `Nama Sekolah` dan `Banyak Murid`, yang di mana proses pendaftarannya SEKALI setiap\nSUBMIT.\n\nPerlu diperhatikan bahwa maksimum pendataannya HANYA sebesar `128` Sekolah saja, dan SATU data Sekolah yang sudah ada TIDAK DAPAT terdaftarkan\nkembali, kecuali diubah (dalam menu tersendiri) atau dihapus terlebih dahulu!",
    };
    const char *ButtonTexts[2] = {"Reset...", "Ubahkan..."};
    
    Rectangle ResetButton = {(SCREEN_WIDTH / 2) - ResetSubmitButtonSize.x - (ResetSubmitButtonGap / 2), SchoolsManagementInputBox_EDIT[1].Box.y + ButtonsGap, ResetSubmitButtonSize.x, ResetSubmitButtonSize.y};
    Rectangle SubmitButton = {ResetButton.x + ResetSubmitButtonSize.x + ResetSubmitButtonGap, ResetButton.y, ResetSubmitButtonSize.x, ResetSubmitButtonSize.y};
    Rectangle Buttons[2] = {ResetButton, SubmitButton};
    Color ButtonColor[2] = { 0 }, TextColor[2] = { 0 };

    TextObject InputBoxHelp[2] = {
        CreateTextObject(GIFont, DataTexts[1], (SchoolDataInputBoxSize.y / 2), SPACING_WIDER, (Vector2){SchoolsManagementInputBox_EDIT[0].Box.x + 10, SchoolsManagementInputBox_EDIT[0].Box.y + 15}, 0, false),
        CreateTextObject(GIFont, DataTexts[2], (SchoolDataInputBoxSize.y / 2), SPACING_WIDER, (Vector2){SchoolsManagementInputBox_EDIT[1].Box.x + 10, SchoolsManagementInputBox_EDIT[1].Box.y + 15}, 0, false)
    };
    TextObject MaxCharsInfo[2] = {
        CreateTextObject(GIFont, DataTexts[3], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER, (Vector2){SchoolsManagementInputBox_EDIT[0].Box.x + SchoolsManagementInputBox_EDIT[0].Box.width - (MeasureTextEx(GIFont, DataTexts[3], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER).x) - 15, SchoolsManagementInputBox_EDIT[0].Box.y + (SchoolDataInputBoxSize.y / 3)}, 0, false),
        CreateTextObject(GIFont, DataTexts[4], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER, (Vector2){SchoolsManagementInputBox_EDIT[1].Box.x + SchoolsManagementInputBox_EDIT[1].Box.width - (MeasureTextEx(GIFont, DataTexts[4], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER).x) - 15, SchoolsManagementInputBox_EDIT[1].Box.y + (SchoolDataInputBoxSize.y / 3)}, 0, false)
    };

    TextObject EditSchoolMessage = CreateTextObject(GIFont, DataTexts[0], FONT_H3, SPACING_WIDER, (Vector2){0, 240 + 40}, 0, true);
    TextObject SuccessMessage = CreateTextObject(GIFont, DataTexts[5], FONT_H3, SPACING_WIDER, (Vector2){0, EditSchoolMessage.TextPosition.y}, 40, true);
    TextObject FirstErrorMessage = CreateTextObject(GIFont, DataTexts[6], FONT_H3, SPACING_WIDER, (Vector2){0, EditSchoolMessage.TextPosition.y}, 40, true);
    TextObject SecondErrorMessage = CreateTextObject(GIFont, DataTexts[7], FONT_H3, SPACING_WIDER, (Vector2){0, EditSchoolMessage.TextPosition.y}, 40, true);
    TextObject HelpInfo = CreateTextObject(GIFont, DataTexts[8], FONT_H5, SPACING_WIDER, (Vector2){100, ResetButton.y}, 75 + ResetButton.height, false);
    
    SchoolsManagementInputBox_EDIT[0].Box = (Rectangle){(SCREEN_WIDTH / 2) - (SchoolDataInputBoxSize.x / 2), 480, SchoolDataInputBoxSize.x, SchoolDataInputBoxSize.y};
    SchoolsManagementInputBox_EDIT[1].Box = (Rectangle){(SCREEN_WIDTH / 2) - (SchoolDataInputBoxSize.x / 2), SchoolsManagementInputBox_EDIT[0].Box.y + ButtonsGap, SchoolDataInputBoxSize.x, SchoolDataInputBoxSize.y};

    DrawManageViewBorder(0);
    DrawTextEx(GIFont, EditSchoolMessage.TextFill, EditSchoolMessage.TextPosition, EditSchoolMessage.FontData.x, EditSchoolMessage.FontData.y, PINK);
    DrawTextEx(GIFont, HelpInfo.TextFill, HelpInfo.TextPosition, HelpInfo.FontData.x, HelpInfo.FontData.y, BEIGE);
    DrawRectangleRec(SchoolsManagementInputBox_EDIT[0].Box, GRAY);
    DrawRectangleRec(SchoolsManagementInputBox_EDIT[1].Box, GRAY);

    if (HasClicked[5]) {
        if (!SchoolDataAlreadyExists) {
            if (SchoolsManagementInputBox_EDIT_AllValid) {
                DrawTextEx(GIFont, FirstErrorMessage.TextFill, FirstErrorMessage.TextPosition, FirstErrorMessage.FontData.x, FirstErrorMessage.FontData.y, Fade(BLACK, 0.0f));
                DrawTextEx(GIFont, SecondErrorMessage.TextFill, SecondErrorMessage.TextPosition, SecondErrorMessage.FontData.x, SecondErrorMessage.FontData.y, Fade(BLACK, 0.0f));
                DrawTextEx(GIFont, SuccessMessage.TextFill, SuccessMessage.TextPosition, SuccessMessage.FontData.x, SuccessMessage.FontData.y, GREEN);
            } else {
                DrawTextEx(GIFont, SuccessMessage.TextFill, SuccessMessage.TextPosition, SuccessMessage.FontData.x, SuccessMessage.FontData.y, Fade(BLACK, 0.0f));
                DrawTextEx(GIFont, SecondErrorMessage.TextFill, SecondErrorMessage.TextPosition, SecondErrorMessage.FontData.x, SecondErrorMessage.FontData.y, Fade(BLACK, 0.0f));
                DrawTextEx(GIFont, FirstErrorMessage.TextFill, FirstErrorMessage.TextPosition, FirstErrorMessage.FontData.x, FirstErrorMessage.FontData.y, RED);
            }
        } else {
            DrawTextEx(GIFont, FirstErrorMessage.TextFill, FirstErrorMessage.TextPosition, FirstErrorMessage.FontData.x, FirstErrorMessage.FontData.y, Fade(BLACK, 0.0f));
            DrawTextEx(GIFont, SuccessMessage.TextFill, SuccessMessage.TextPosition, SuccessMessage.FontData.x, SuccessMessage.FontData.y, Fade(BLACK, 0.0f));
            DrawTextEx(GIFont, SecondErrorMessage.TextFill, SecondErrorMessage.TextPosition, SecondErrorMessage.FontData.x, SecondErrorMessage.FontData.y, RED);
        }
    }

    Vector2 Mouse = GetMousePosition();
    for (int i = 0; i < 2; i++) {
        if (CheckCollisionPointRec(Mouse, Buttons[i])) {
            HoverTransition[1006 + i] = CustomLerp(HoverTransition[1006 + i], 1.0f, 0.1f);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (i == 0) ResetInputs(SchoolsManagementInputBox_EDIT, (Vector2){0, 2});
                if (i == 1) {
                    SchoolsManagementInputBox_EDIT_AllValid = true;
                    SchoolDataAlreadyExists = false;

                    for (int j = 0; j < 2; j++) {
                        if (j == 0) { SchoolsManagementInputBox_EDIT[j].IsValid = (strlen(SchoolsManagementInputBox_EDIT[j].Text) >= 2); }
                        else if (j == 1) { SchoolsManagementInputBox_EDIT[j].IsValid = (atoi(SchoolsManagementInputBox_EDIT[j].Text) >= 10 && atoi(SchoolsManagementInputBox_EDIT[j].Text) <= 9999); }

                        if (!SchoolsManagementInputBox_EDIT[j].IsValid) { SchoolsManagementInputBox_EDIT_AllValid = false; SchoolDataAlreadyExists = false; }
                    }

                    int SchoolIndex = -1;
                    strcpy(NewSchoolName, SchoolsManagementInputBox_EDIT[0].Text);
                    ReadSchoolsData(Schools);

                    SchoolsManagementInputBox_EDIT_AllValid = false;    // Default to false
                    SchoolDataAlreadyExists = false;                    // Default to false
                    for (int s = 0; s < MAX_ACCOUNT_LIMIT; s++) {
                        if (strcmp(OriginalSchools[s].Name, OldSchoolName) == 0) { SchoolIndex = s; break; }
                    }
                    
                    if (SchoolIndex == -1) { SchoolsManagementInputBox_EDIT_AllValid = false; SchoolDataAlreadyExists = false; }
                    if (strcmp(OldSchoolName, NewSchoolName) != 0) {
                        for (int s = 0; s < MAX_ACCOUNT_LIMIT; s++) {
                            if (s != SchoolIndex && strcmp(OriginalSchools[s].Name, NewSchoolName) == 0) {
                                SchoolDataAlreadyExists = true;
                                break;
                            }
                        }
                    }
                    
                    SchoolsManagementInputBox_EDIT_AllValid = true;
                    if (SchoolsManagementInputBox_EDIT_AllValid && !SchoolDataAlreadyExists) {
                        if (strcmp(OldSchoolName, NewSchoolName) != 0) {
                            strcpy(OriginalSchools[SchoolIndex].Name, NewSchoolName);
                            SchoolDataAlreadyExists = false;
                        } strcpy(OriginalSchools[SchoolIndex].Students, SchoolsManagementInputBox_EDIT[1].Text);
                        
                        SyncSchoolsFromOriginalSchoolData(Schools, OriginalSchools);
                        WriteSchoolsData(OriginalSchools);
                    }
                    
                    HasClicked[5] = true;
                }
                
                PlaySound(ButtonClickSFX);
                Transitioning = true;
                ScreenFade = 1.0f;
            }
        } else HoverTransition[1006 + i] = CustomLerp(HoverTransition[1006 + i], 0.0f, 0.1f);
        
        if (i == 0) { // Reset button
            ButtonColor[i] = (HoverTransition[1006 + i] > 0.5f) ? RED : LIGHTGRAY;
            TextColor[i] = (HoverTransition[1006 + i] > 0.5f) ? WHITE : BLACK;
        } else if (i == 1) { // Submit button
            ButtonColor[i] = (HoverTransition[1006 + i] > 0.5f) ? GREEN : LIGHTGRAY;
            TextColor[i] = (HoverTransition[1006 + i] > 0.5f) ? WHITE : BLACK;
        }

        float scale = 1.0f + HoverTransition[1006 + i] * 0.1f;
        
        float newWidth = Buttons[i].width * scale;
        float newHeight = Buttons[i].height * scale;
        float newX = Buttons[i].x - (newWidth - Buttons[i].width) / 2;
        float newY = Buttons[i].y - (newHeight - Buttons[i].height) / 2;
        
        DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, ButtonColor[i]);
        
        Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[i], (FONT_P * scale), SPACING_WIDER);
        int textX = newX + (newWidth / 2) - (textWidth.x / 2);
        int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
        DrawTextEx(GIFont, ButtonTexts[i], (Vector2){textX, textY}, (FONT_P * scale), SPACING_WIDER, TextColor[i]);
    }
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        ActiveBox = -1;  // Reset active box
        for (int i = 0; i < 2; i++) {
            if (CheckCollisionPointRec(Mouse, SchoolsManagementInputBox_EDIT[i].Box)) {
                ActiveBox = i;
                break;
            }
        }
    }

    if (ActiveBox != -1) {
        int Key = GetCharPressed();
        if (ActiveBox == 0) {
            while (Key > 0) {
                if (strlen(SchoolsManagementInputBox_EDIT[ActiveBox].Text) < MAX_INPUT_LENGTH) {
                    int len = strlen(SchoolsManagementInputBox_EDIT[ActiveBox].Text);
                    SchoolsManagementInputBox_EDIT[ActiveBox].Text[len] = (char)Key;
                    SchoolsManagementInputBox_EDIT[ActiveBox].Text[len + 1] = '\0';
                } Key = GetCharPressed();
            }
        } else if (ActiveBox == 1) {
            while (Key >= '0' && Key <= '9') {
                if (strlen(SchoolsManagementInputBox_EDIT[ActiveBox].Text) < 4) {
                    int len = strlen(SchoolsManagementInputBox_EDIT[ActiveBox].Text);
                    SchoolsManagementInputBox_EDIT[ActiveBox].Text[len] = (char)Key;
                    SchoolsManagementInputBox_EDIT[ActiveBox].Text[len + 1] = '\0';
                } Key = GetCharPressed();
            }
        }

        if (IsKeyPressed(KEY_BACKSPACE) && strlen(SchoolsManagementInputBox_EDIT[ActiveBox].Text) > 0) {
            SchoolsManagementInputBox_EDIT[ActiveBox].Text[strlen(SchoolsManagementInputBox_EDIT[ActiveBox].Text) - 1] = '\0';
        } else if (IsKeyDown(KEY_BACKSPACE) && strlen(SchoolsManagementInputBox_EDIT[ActiveBox].Text) > 0) {
            CurrentFrameTime += GetFrameTime();
            if (CurrentFrameTime >= 0.5f) {
                SchoolsManagementInputBox_EDIT[ActiveBox].Text[strlen(SchoolsManagementInputBox_EDIT[ActiveBox].Text) - 1] = '\0';
            }
        }
        
        if (IsKeyReleased(KEY_BACKSPACE)) CurrentFrameTime = 0;
    }

    FrameCounter++; // Increment frame counter for cursor blinking
    for (int i = 0; i < 2; i++) {
        Color boxColor = GRAY;
        if (!SchoolsManagementInputBox_EDIT[i].IsValid && HasClicked[5]) { boxColor = RED; }
        
        if (i == ActiveBox) boxColor = SKYBLUE; // Active input
        else if (CheckCollisionPointRec(Mouse, SchoolsManagementInputBox_EDIT[i].Box)) boxColor = YELLOW; // Hover effect

        DrawRectangleRec(SchoolsManagementInputBox_EDIT[i].Box, boxColor);
        DrawTextEx(GIFont, SchoolsManagementInputBox_EDIT[i].Text, (Vector2){SchoolsManagementInputBox_EDIT[i].Box.x + 10, SchoolsManagementInputBox_EDIT[i].Box.y + 15}, (SchoolDataInputBoxSize.y / 2), SPACING_WIDER, BLACK);

        if (i == ActiveBox && (FrameCounter / CURSOR_BLINK_SPEED) % 2 == 0) {
            int textWidth = MeasureTextEx(GIFont, SchoolsManagementInputBox_EDIT[i].Text, (SchoolDataInputBoxSize.y / 2), SPACING_WIDER).x;
            DrawTextEx(GIFont, "|", (Vector2){SchoolsManagementInputBox_EDIT[i].Box.x + 10 + textWidth, SchoolsManagementInputBox_EDIT[i].Box.y + 10}, (SchoolDataInputBoxSize.y / 2) + 10, SPACING_WIDER, BLACK);
        }
        
        DrawTextEx(GIFont, MaxCharsInfo[i].TextFill, MaxCharsInfo[i].TextPosition, MaxCharsInfo[i].FontData.x, MaxCharsInfo[i].FontData.y, BLACK);
        if (strlen(SchoolsManagementInputBox_EDIT[i].Text) == 0 && i != ActiveBox) {
            DrawTextEx(GIFont, InputBoxHelp[i].TextFill, InputBoxHelp[i].TextPosition, InputBoxHelp[i].FontData.x, InputBoxHelp[i].FontData.y, Fade(BLACK, 0.6));
        }
    }
}

void GovernmentManageSchoolsMenu_DELETE(void) {
    char ReturnMenuState[256] = { 0 };
    
    PreviousMenu = MENU_MAIN_GOVERNMENT_SchoolManagement;
    DrawPreviousMenuButton();

    GetMenuState(CurrentMenu, ReturnMenuState, false);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H2, SPACING_WIDER, (Vector2){0, 60}, 0, true);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

    const char *DataTexts[] = {
        "Apakah Anda yakin untuk MENGHAPUS data Sekolah terkait?",
        "Proses ini akan berdampak secara permanen apabila dilanjutkan, dimohon untuk berhati-hati!",

        "Nama Sekolah:",
        "Banyak Murid:",
        "Afiliasi Vendor (atau Catering):",

        "Anda telah berhasil menghapus data Sekolah terkait dari program D'Magis ini!",
        "Setelah ini Anda akan di bawa kembali ke menu `[MANAGE]: Data Sekolah`...",
        "Tekan di mana saja untuk melanjutkan...",

        "Sekolah terkait TIDAK sedang dalam masa afiliasi terhadap pihak Vendor, maka tidak mengapa dilakukan tindakan penghapusan",
        "data Sekolah tersebut karena tidak berdampak lain pada Vendor yang terikat.",
        
        "Sekolah terkait SEDANG dalam masa afiliasi terhadap pihak Vendor yang terdata, yang mana proses penghapusan data sekolah",
        "terkait akan menyebabkan hilangnya koneksi dengan Vendor tersebut. Apabila data Sekolah ini terhapus, maka pada Vendor yang",
        "bersangkutan akan hilang status afiliasinya secara PAKSA. Dengan demikian, mohon untuk dipertimbangkan kembali..."
    };
    const char *ButtonTexts[2] = {"Tidak...", "Iya..."};
    
    int ResetSubmitButtonGap = 40, ButtonsGap = 80;
    Vector2 SchoolDeletionInputBoxSize = {900, 60};
    Vector2 ResetSubmitButtonSize = {(SchoolDeletionInputBoxSize.x / 2) - (ButtonsGap / 4), 60};
    
    // //
    SchoolsManagementInputBox_DELETE[0].Box.x = (SCREEN_WIDTH / 2) - (SchoolDeletionInputBoxSize.x / 2);
    SchoolsManagementInputBox_DELETE[0].Box.y = 500;
    SchoolsManagementInputBox_DELETE[0].Box.width = SchoolDeletionInputBoxSize.x;
    SchoolsManagementInputBox_DELETE[0].Box.height = SchoolDeletionInputBoxSize.y;

    SchoolsManagementInputBox_DELETE[1].Box.x = SchoolsManagementInputBox_DELETE[0].Box.x;
    SchoolsManagementInputBox_DELETE[1].Box.y = SchoolsManagementInputBox_DELETE[0].Box.y + ButtonsGap;
    SchoolsManagementInputBox_DELETE[1].Box.width = SchoolDeletionInputBoxSize.x;
    SchoolsManagementInputBox_DELETE[1].Box.height = SchoolDeletionInputBoxSize.y;

    SchoolsManagementInputBox_DELETE[2].Box.x = SchoolsManagementInputBox_DELETE[1].Box.x;
    SchoolsManagementInputBox_DELETE[2].Box.y = SchoolsManagementInputBox_DELETE[1].Box.y + ButtonsGap;
    SchoolsManagementInputBox_DELETE[2].Box.width = SchoolDeletionInputBoxSize.x;
    SchoolsManagementInputBox_DELETE[2].Box.height = SchoolDeletionInputBoxSize.y;
    // //

    Rectangle ResetButton = {(SCREEN_WIDTH / 2) - ResetSubmitButtonSize.x - (ResetSubmitButtonGap / 2), SchoolsManagementInputBox_DELETE[2].Box.y - (ButtonsGap / 4), ResetSubmitButtonSize.x, ResetSubmitButtonSize.y};
    Rectangle SubmitButton = {ResetButton.x + ResetSubmitButtonSize.x + ResetSubmitButtonGap, ResetButton.y, ResetSubmitButtonSize.x, ResetSubmitButtonSize.y};
    Rectangle Buttons[2] = {ResetButton, SubmitButton};
    Color ButtonColor[2] = { 0 }, TextColor[2] = { 0 };

    TextObject ConfirmSchoolDeletionMessage[2] = {
        CreateTextObject(GIFont, DataTexts[0], FONT_H2, SPACING_WIDER, (Vector2){0, 240}, 30, true),
        CreateTextObject(GIFont, DataTexts[1], FONT_H3, SPACING_WIDER, ConfirmSchoolDeletionMessage[0].TextPosition, 45, true)
    };

    if (strlen(SchoolsManagementInputBox_DELETE[2].Text) == 0) strcpy(SchoolsManagementInputBox_DELETE[2].Text, "(tidak ter-afiliasi oleh Vendor manapun...)");
    TextObject DataConfirmationInfo[6] = {
        CreateTextObject(GIFont, DataTexts[2], FONT_P, SPACING_WIDER, (Vector2){SchoolsManagementInputBox_DELETE[0].Box.x, SchoolsManagementInputBox_DELETE[0].Box.y}, 0, false),
        CreateTextObject(GIFont, DataTexts[3], FONT_P, SPACING_WIDER, (Vector2){SchoolsManagementInputBox_DELETE[1].Box.x, SchoolsManagementInputBox_DELETE[1].Box.y}, -40, false),
        CreateTextObject(GIFont, DataTexts[4], FONT_P, SPACING_WIDER, (Vector2){SchoolsManagementInputBox_DELETE[2].Box.x, SchoolsManagementInputBox_DELETE[2].Box.y}, -80, false),

        CreateTextObject(GIFont, SchoolsManagementInputBox_DELETE[0].Text, FONT_P, SPACING_WIDER, (Vector2){DataConfirmationInfo[0].TextPosition.x + SchoolsManagementInputBox_DELETE[0].Box.x - 40, DataConfirmationInfo[0].TextPosition.y}, 0, false),
        CreateTextObject(GIFont, SchoolsManagementInputBox_DELETE[1].Text, FONT_P, SPACING_WIDER, (Vector2){DataConfirmationInfo[1].TextPosition.x + SchoolsManagementInputBox_DELETE[1].Box.x - 40, DataConfirmationInfo[1].TextPosition.y}, 0, false),
        CreateTextObject(GIFont, SchoolsManagementInputBox_DELETE[2].Text, FONT_P, SPACING_WIDER, (Vector2){DataConfirmationInfo[2].TextPosition.x + SchoolsManagementInputBox_DELETE[2].Box.x - 40, DataConfirmationInfo[2].TextPosition.y}, 0, false),
    };

    TextObject AffiliationStatusMessage[5] = {
        CreateTextObject(GIFont, DataTexts[8], FONT_P, SPACING_WIDER, (Vector2){90, Buttons[0].y + 170}, 40, false),
        CreateTextObject(GIFont, DataTexts[9], FONT_P, SPACING_WIDER, (Vector2){90, Buttons[0].y + 170}, 80, false),
        CreateTextObject(GIFont, DataTexts[10], FONT_P, SPACING_WIDER, (Vector2){90, Buttons[0].y + 170}, 0, false),
        CreateTextObject(GIFont, DataTexts[11], FONT_P, SPACING_WIDER, (Vector2){90, Buttons[0].y + 170}, 40, false),
        CreateTextObject(GIFont, DataTexts[12], FONT_P, SPACING_WIDER, (Vector2){90, Buttons[0].y + 170}, 80, false),
    };

    TextObject SuccessMessage[3] = {
        CreateTextObject(GIFont, DataTexts[5], FONT_H2, SPACING_WIDER, (Vector2){0, 520}, 0, true),
        CreateTextObject(GIFont, DataTexts[6], FONT_H3, SPACING_WIDER, SuccessMessage[0].TextPosition, 40, true),
        CreateTextObject(GIFont, DataTexts[7], FONT_H3, SPACING_WIDER, SuccessMessage[1].TextPosition, 60, true)
    };

    DrawManageViewBorder(0);

    Vector2 Mouse = GetMousePosition();
    if (HasClicked[6]) {
        DrawTextEx(GIFont, SuccessMessage[0].TextFill, SuccessMessage[0].TextPosition, SuccessMessage[0].FontData.x, SuccessMessage[0].FontData.y, GREEN);
        DrawTextEx(GIFont, SuccessMessage[1].TextFill, SuccessMessage[1].TextPosition, SuccessMessage[1].FontData.x, SuccessMessage[1].FontData.y, SKYBLUE);
        DrawTextEx(GIFont, SuccessMessage[2].TextFill, SuccessMessage[2].TextPosition, SuccessMessage[2].FontData.x, SuccessMessage[2].FontData.y, WHITE);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            PreviousMenu = MENU_MAIN_GOVERNMENT_SchoolManagement;
            NextMenu = MENU_MAIN_GOVERNMENT_SchoolManagement;
            CurrentMenu = NextMenu;

            PlaySound(ButtonClickSFX);
            SuccessfulWriteUserAccount = false;
            HasClicked[6] = false;
            Transitioning = true;
            ScreenFade = 1.0f;
        }
    } else {
        for (int i = 0; i < 2; i++) {
            DrawTextEx(GIFont, ConfirmSchoolDeletionMessage[i].TextFill, ConfirmSchoolDeletionMessage[i].TextPosition, ConfirmSchoolDeletionMessage[i].FontData.x, ConfirmSchoolDeletionMessage[i].FontData.y, (i % 2 == 0) ? ORANGE : PINK);
        }

        for (int p = 0; p < 2; p++) {
            if (strcmp(SchoolsManagementInputBox_DELETE[2].Text, "(tidak ter-afiliasi oleh Vendor manapun...)") == 0) {
                DrawTextEx(GIFont, AffiliationStatusMessage[p].TextFill, AffiliationStatusMessage[p].TextPosition, AffiliationStatusMessage[p].FontData.x, AffiliationStatusMessage[p].FontData.y, GOLD);
            }
        }

        for (int i = 0; i < 2; i++) {
            if (CheckCollisionPointRec(Mouse, Buttons[i])) {
                HoverTransition[i + 977] = CustomLerp(HoverTransition[i + 977], 1.0f, 0.1f);
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    if (i == 0) NextMenu = MENU_MAIN_GOVERNMENT_SchoolManagement;
                    if (i == 1) {
                        int SaveDeletedIndex = -1;

                        ReadSchoolsData(Schools);
                        for (int s = 0; s < MAX_ACCOUNT_LIMIT; s++) {
                            if (strcmp(OriginalSchools[s].Name, SchoolsManagementInputBox_DELETE[0].Text) == 0 &&
                                strcmp(OriginalSchools[s].Students, SchoolsManagementInputBox_DELETE[1].Text) == 0) {
                                
                                if (strlen(OriginalSchools[s].AffiliatedVendor) == 0) { SaveDeletedIndex = s; break; }
                            }
                        }

                        if (SaveDeletedIndex != -1) {
                            for (int s = SaveDeletedIndex; s < MAX_ACCOUNT_LIMIT - 1; s++) { OriginalSchools[s] = OriginalSchools[s + 1]; }
                            memset(&OriginalSchools[MAX_ACCOUNT_LIMIT - 1], 0, sizeof(SchoolDatabase));
                            SyncSchoolsFromOriginalSchoolData(Schools, OriginalSchools);
                            WriteSchoolsData(OriginalSchools);
                        }
                        
                        ReadUserAccount(Vendors);
                        for (int j = 0; j < MAX_ACCOUNT_LIMIT; j++) {
                            if (OriginalVendors[j].SaveSchoolDataIndex > SaveDeletedIndex) {
                                OriginalVendors[j].SaveSchoolDataIndex--;  // Shift affiliated indexes to match the new structure
                            }
                        }
                        
                        SyncVendorsFromOriginalVendorData(Vendors, OriginalVendors);
                        WriteUserAccount(OriginalVendors);

                        HasClicked[6] = true;
                    }
                    
                    PlaySound(ButtonClickSFX);
                    Transitioning = true;
                    ScreenFade = 1.0f;
                }
            } else HoverTransition[i + 977] = CustomLerp(HoverTransition[i + 977], 0.0f, 0.1f);
            
            if (i == 0) { // Reset button
                ButtonColor[i] = (HoverTransition[i + 977] > 0.5f) ? RED : LIGHTGRAY;
                TextColor[i] = (HoverTransition[i + 977] > 0.5f) ? WHITE : BLACK;
            } else if (i == 1) { // Submit button
                ButtonColor[i] = (HoverTransition[i + 977] > 0.5f) ? GREEN : LIGHTGRAY;
                TextColor[i] = (HoverTransition[i + 977] > 0.5f) ? WHITE : BLACK;
            }

            float scale = 1.0f + HoverTransition[i + 977] * 0.1f;
            
            float newWidth = Buttons[i].width * scale;
            float newHeight = Buttons[i].height * scale;
            float newX = Buttons[i].x - (newWidth - Buttons[i].width) / 2;
            float newY = Buttons[i].y - (newHeight - Buttons[i].height) / 2;
            
            if (strcmp(SchoolsManagementInputBox_DELETE[2].Text, "(tidak ter-afiliasi oleh Vendor manapun...)") == 0) {
                DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, ButtonColor[i]);
                
                Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[i], (FONT_P * scale), SPACING_WIDER);
                int textX = newX + (newWidth / 2) - (textWidth.x / 2);
                int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
                DrawTextEx(GIFont, ButtonTexts[i], (Vector2){textX, textY}, (FONT_P * scale), SPACING_WIDER, TextColor[i]);
            } else {
                for (int p = 2; p < 5; p++) {
                    DrawTextEx(GIFont, AffiliationStatusMessage[p].TextFill, AffiliationStatusMessage[p].TextPosition, AffiliationStatusMessage[p].FontData.x, AffiliationStatusMessage[p].FontData.y, RED);
                }
            }
        }

        DrawTextEx(GIFont, DataConfirmationInfo[0].TextFill, DataConfirmationInfo[0].TextPosition, DataConfirmationInfo[0].FontData.x, DataConfirmationInfo[0].FontData.y, WHITE);
        DrawTextEx(GIFont, DataConfirmationInfo[3].TextFill, DataConfirmationInfo[3].TextPosition, DataConfirmationInfo[3].FontData.x, DataConfirmationInfo[3].FontData.y, PURPLE);
        
        DrawTextEx(GIFont, DataConfirmationInfo[1].TextFill, DataConfirmationInfo[1].TextPosition, DataConfirmationInfo[1].FontData.x, DataConfirmationInfo[1].FontData.y, WHITE);
        DrawTextEx(GIFont, DataConfirmationInfo[4].TextFill, DataConfirmationInfo[4].TextPosition, DataConfirmationInfo[4].FontData.x, DataConfirmationInfo[4].FontData.y, BEIGE);
        
        DrawTextEx(GIFont, DataConfirmationInfo[2].TextFill, DataConfirmationInfo[2].TextPosition, DataConfirmationInfo[2].FontData.x, DataConfirmationInfo[2].FontData.y, WHITE);
        DrawTextEx(GIFont, DataConfirmationInfo[5].TextFill, DataConfirmationInfo[5].TextPosition, DataConfirmationInfo[5].FontData.x, DataConfirmationInfo[5].FontData.y, YELLOW);
    }
}

void GovernmentManageBilateralsMenu(void) {
    char ReturnMenuState[256] = { 0 };
    int LeftMargin = 40, TopMargin = 20;
    Vector2 EditDeleteButtonSize = {180, 50};
    
    Rectangle AFFILIATE_GOTOPAGE_Buttons[6] = {
        {SCREEN_WIDTH - 240 - 15, 240 + 48, EditDeleteButtonSize.x, EditDeleteButtonSize.y}, 
        {AFFILIATE_GOTOPAGE_Buttons[0].x, AFFILIATE_GOTOPAGE_Buttons[0].y + 144, EditDeleteButtonSize.x, EditDeleteButtonSize.y}, 
        {AFFILIATE_GOTOPAGE_Buttons[1].x, AFFILIATE_GOTOPAGE_Buttons[1].y + 144, EditDeleteButtonSize.x, EditDeleteButtonSize.y}, 
        {AFFILIATE_GOTOPAGE_Buttons[2].x, AFFILIATE_GOTOPAGE_Buttons[2].y + 144, EditDeleteButtonSize.x, EditDeleteButtonSize.y}, 
        {AFFILIATE_GOTOPAGE_Buttons[3].x, AFFILIATE_GOTOPAGE_Buttons[3].y + 144, EditDeleteButtonSize.x, EditDeleteButtonSize.y}, 
        {SCREEN_WIDTH - 180, 160, 120, 60}
    };
    Color AFFILIATE_GOTOPAGE_ButtonColor[6], AFFILIATE_GOTOPAGE_TextColor[6];
    const char *AFFILIATE_GOTOPAGE_ButtonTexts[] = {"[+] Afiliasikan...", "Tuju!"};

    const char *SortingSettingButtonText = "[@] Urutkan...";
    Rectangle SortingSettingButton = {60, AFFILIATE_GOTOPAGE_Buttons[5].y, (AFFILIATE_GOTOPAGE_Buttons[5].width * 2), AFFILIATE_GOTOPAGE_Buttons[5].height};
    Color SortingSettingButtonColor, SortingSettingTextColor;

    Rectangle EditDeleteButtons[10] = {
        {(SCREEN_WIDTH - 330) + 75, 240 + 18, EditDeleteButtonSize.x, EditDeleteButtonSize.y},
        {EditDeleteButtons[0].x, EditDeleteButtons[0].y + 60, EditDeleteButtonSize.x, EditDeleteButtonSize.y},

        {EditDeleteButtons[0].x, (EditDeleteButtons[0].y + 144), EditDeleteButtonSize.x, EditDeleteButtonSize.y},
        {EditDeleteButtons[0].x, EditDeleteButtons[2].y + 60, EditDeleteButtonSize.x, EditDeleteButtonSize.y},
        
        {EditDeleteButtons[0].x, (EditDeleteButtons[2].y + 144), EditDeleteButtonSize.x, EditDeleteButtonSize.y},
        {EditDeleteButtons[0].x, EditDeleteButtons[4].y + 60, EditDeleteButtonSize.x, EditDeleteButtonSize.y},

        {EditDeleteButtons[0].x, (EditDeleteButtons[4].y + 144), EditDeleteButtonSize.x, EditDeleteButtonSize.y},
        {EditDeleteButtons[0].x, EditDeleteButtons[6].y + 60, EditDeleteButtonSize.x, EditDeleteButtonSize.y},
        
        {EditDeleteButtons[0].x, (EditDeleteButtons[6].y + 144), EditDeleteButtonSize.x, EditDeleteButtonSize.y},
        {EditDeleteButtons[0].x, EditDeleteButtons[8].y + 60, EditDeleteButtonSize.x, EditDeleteButtonSize.y}
    };
    Color EditDeleteButtonColor[10], EditDeleteTextColor[10];
    const char *EditDeleteButtonTexts[] = {"[?] Ganti...", "[-] Batalkan..."};

    PreviousMenu = MENU_MAIN_GOVERNMENT;
    DrawPreviousMenuButton();

    GetMenuState(CurrentMenu, ReturnMenuState, false);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H2, SPACING_WIDER, (Vector2){0, 60}, 0, true);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

    const char *DataTexts[] = {
        "Mohon maaf, saat ini sistem tidak dapat menampilkan data yang diperlukan untuk proses bilaretal D'Magis, dikarenakan:",
        "1. Keperluan data dari pihak Vendor (atau Catering) BELUM tersedia, dan",
        "2. Keperluan data dari pihak Sekolah BELUM tersedia.",
        "Pastikan kondisi dari kedua ketentuan di atas untuk DIPENUHI terlebih dahulu, dikarenakan proses bilateral ini membutuhkan",
        "kerja sama dari kedua belah pihak dan TANPA PAKSAAN sekalipun.",
        "Untuk menyelesaikan masalah ini, dari pihak Anda dapat melakukan pendaftaran data Sekolah baru pada menu `[MANAGE]: Data Sekolah`,",
        "sedangkan teruntuk data para Vendor diharuskan menunggu inisiatif dari merekanya sendiri...",
        "Terima kasih atas kerja samanya, Pemerintah setempat..."
    };
    TextObject UnfulfilledRequestsMessage[8] = {
        CreateTextObject(GIFont, DataTexts[0], FONT_H4, SPACING_WIDER, (Vector2){0, 300}, 0, true),
        CreateTextObject(GIFont, DataTexts[1], FONT_H3, SPACING_WIDER, (Vector2){0, UnfulfilledRequestsMessage[0].TextPosition.y}, 40, true),
        CreateTextObject(GIFont, DataTexts[2], FONT_H3, SPACING_WIDER, (Vector2){0, UnfulfilledRequestsMessage[1].TextPosition.y}, 40, true),
        
        CreateTextObject(GIFont, DataTexts[3], FONT_H5, SPACING_WIDER, (Vector2){0, (SCREEN_HEIGHT / 2)}, 0, true),
        CreateTextObject(GIFont, DataTexts[4], FONT_H5, SPACING_WIDER, (Vector2){0, UnfulfilledRequestsMessage[3].TextPosition.y}, 30, true),
        CreateTextObject(GIFont, DataTexts[5], FONT_H5, SPACING_WIDER, (Vector2){0, UnfulfilledRequestsMessage[4].TextPosition.y}, 60, true),
        CreateTextObject(GIFont, DataTexts[6], FONT_H5, SPACING_WIDER, (Vector2){0, UnfulfilledRequestsMessage[5].TextPosition.y}, 30, true),
        
        CreateTextObject(GIFont, DataTexts[7], FONT_H3, SPACING_WIDER, (Vector2){0, SCREEN_HEIGHT - 200}, 0, true),
    };

    char SAD_No[16] = { 0 }, SAD_GeneratedSecondLineMeasurement[32] = { 0 };
    TextObject SAD_Numberings[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_FirstLine[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_SchoolName[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_SecondLine[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_VendorName[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_ThirdLine[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_ManyStudents[MAX_ACCOUNT_LIMIT] = { 0 };
    
    GoToPageInputBox[1].Box = (Rectangle){AFFILIATE_GOTOPAGE_Buttons[5].x - AFFILIATE_GOTOPAGE_Buttons[5].width + 20, AFFILIATE_GOTOPAGE_Buttons[5].y, 80, 60};

    char ShowRegisteredSchoolsText[128] = { 0 };
    const char *PrepareGoToPageGuide = "Pergi ke halaman:";
    const char *PreparePaginationGuide = "Tekan [<] atau [>] di keyboard laptop/PC Anda untuk bernavigasi...";
    char PrepareAffiliationMessage[256] = { 0 };
    char PreparePaginationTextInfo[64] = { 0 };

    snprintf(ShowRegisteredSchoolsText, sizeof(ShowRegisteredSchoolsText), "Sebanyak (%d/%d) data Vendor dan (%d/%d) data Sekolah telah terdata...", ReadVendors, MAX_ACCOUNT_LIMIT, ReadSchools, MAX_ACCOUNT_LIMIT);
    TextObject ShowRegisteredSchools = CreateTextObject(GIFont, ShowRegisteredSchoolsText, FONT_P, SPACING_WIDER, (Vector2){0, 200}, 0, true);
    snprintf(PrepareAffiliationMessage, sizeof(PrepareAffiliationMessage), "Terdapat [%d] data afiliasi antar Vendor dengan Sekolah yang BELUM tercatat.\nApakah Anda ingin menambahkan/mengubah status afiliasi tertentu?", ReadOnlyNonAffiliatedSchools);
    TextObject AffiliationMessage = CreateTextObject(GIFont, PrepareAffiliationMessage, FONT_H5, SPACING_WIDER, (Vector2){60, 980}, 0, false);
    snprintf(PreparePaginationTextInfo, sizeof(PreparePaginationTextInfo), "Halaman %d dari %d", (CurrentPages[1] + 1), (ReadVendors % DISPLAY_PER_PAGE != 0) ? ((ReadVendors / DISPLAY_PER_PAGE) + 1) : (ReadVendors / DISPLAY_PER_PAGE));
    TextObject GoToPageGuide = CreateTextObject(GIFont, PrepareGoToPageGuide, FONT_H5, SPACING_WIDER, (Vector2){GoToPageInputBox[1].Box.x, GoToPageInputBox[1].Box.y - (GoToPageInputBox[1].Box.height / 2)}, 0, false);
    TextObject PaginationInfo = CreateTextObject(GIFont, PreparePaginationTextInfo, FONT_P, SPACING_WIDER, (Vector2){SCREEN_WIDTH - 60 - MeasureTextEx(GIFont, PreparePaginationTextInfo, FONT_P, SPACING_WIDER).x, 980}, 0, false);
    TextObject PaginationGuide = CreateTextObject(GIFont, PreparePaginationGuide, FONT_H5, SPACING_WIDER, (Vector2){SCREEN_WIDTH - 60 - MeasureTextEx(GIFont, PreparePaginationGuide, FONT_H5, SPACING_WIDER).x, PaginationInfo.TextPosition.y}, 40, false);

    ReadVendors = ReadUserAccount(Vendors);
    ReadSchools = ReadSchoolsData(Schools);
    ReadOnlyNonAffiliatedSchools = ReadNonAffiliatedSchools(Schools);
    UsersOnPage = ReadUsersWithPagination(CurrentPages[1], Vendors);
    SchoolsOnPage = ReadSchoolsWithPagination(CurrentPages[1], Schools);
    
    Vector2 Mouse = GetMousePosition();
    if (ReadVendors == 0 || ReadSchools == 0) {
        // DrawManageViewBorder(0);
        for (int i = 0; i < 8; i++) {
            DrawTextEx(GIFont, UnfulfilledRequestsMessage[i].TextFill, UnfulfilledRequestsMessage[i].TextPosition, UnfulfilledRequestsMessage[i].FontData.x, UnfulfilledRequestsMessage[i].FontData.y, (i == 0) ? YELLOW : (i == 1 || i == 2) ? PINK : (i == 3 || i == 4) ? GREEN : (i == 5 || i == 6) ? SKYBLUE : WHITE);
        }
    } else {
        {
            if (CheckCollisionPointRec(Mouse, SortingSettingButton)) {
                HoverTransition[(5 + 940) + 1] = CustomLerp(HoverTransition[(5 + 940) + 1], 1.0f, 0.1f);

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {                
                    NextMenu = MENU_SUBMAIN_SortingOptions;
                    PreviousMenu = CurrentMenu;

                    PlaySound(ButtonClickSFX);
                    Transitioning = true;
                    ScreenFade = 1.0f;
                    GoingBack = true;
                }
            } else HoverTransition[(5 + 940) + 1] = CustomLerp(HoverTransition[(5 + 940) + 1], 0.0f, 0.1f);
            
            SortingSettingButtonColor = (HoverTransition[(5 + 940) + 1] > 0.5f) ? PURPLE : DARKPURPLE;
            SortingSettingTextColor = (HoverTransition[(5 + 940) + 1] > 0.5f) ? BLACK : WHITE;
                
            float scale = 1.0f + HoverTransition[(5 + 940) + 1] * 0.1f;
            float newWidth = SortingSettingButton.width * scale;
            float newHeight = SortingSettingButton.height * scale;
            float newX = SortingSettingButton.x - (newWidth - SortingSettingButton.width) / 2;
            float newY = SortingSettingButton.y - (newHeight - SortingSettingButton.height) / 2;
            
            DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, SortingSettingButtonColor);

            Vector2 textWidth = MeasureTextEx(GIFont, SortingSettingButtonText, (FONT_H3 * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_H3 * scale / 2);
            DrawTextEx(GIFont, SortingSettingButtonText, (Vector2){textX, textY}, (FONT_H3 * scale), SPACING_WIDER, SortingSettingTextColor);
        }

        {
            DrawRectangleRec(GoToPageInputBox[1].Box, GRAY);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (CheckCollisionPointRec(Mouse, GoToPageInputBox[1].Box)) { GoToPageInputBox[1].IsActive = true; }
                else { GoToPageInputBox[1].IsActive = false; }
            }

            if (GoToPageInputBox[1].IsActive) {
                int Key = GetCharPressed();
                while (Key >= '0' && Key <= '9') {
                    if (strlen(GoToPageInputBox[1].Text) < 2) {
                        int len = strlen(GoToPageInputBox[1].Text);
                        GoToPageInputBox[1].Text[len] = (char)Key;
                        GoToPageInputBox[1].Text[len + 1] = '\0';
                    } Key = GetCharPressed();
                }

                if (IsKeyPressed(KEY_BACKSPACE) && strlen(GoToPageInputBox[1].Text) > 0) {
                    GoToPageInputBox[1].Text[strlen(GoToPageInputBox[1].Text) - 1] = '\0';
                }
            }

            FrameCounter++; // Increment frame counter for cursor blinking
            Color boxColor = GRAY;    
            if (GoToPageInputBox[1].IsActive) boxColor = SKYBLUE; // Active input
            else if (CheckCollisionPointRec(Mouse, GoToPageInputBox[1].Box)) boxColor = YELLOW; // Hover effect

            DrawRectangleRec(GoToPageInputBox[1].Box, boxColor);
            DrawTextEx(GIFont, GoToPageInputBox[1].Text, (Vector2){GoToPageInputBox[1].Box.x + 5, GoToPageInputBox[1].Box.y + 5}, FONT_H1, SPACING_WIDER, BLACK);

            if (GoToPageInputBox[1].IsActive && (FrameCounter / CURSOR_BLINK_SPEED) % 2 == 0) {
                int textWidth = MeasureTextEx(GIFont, GoToPageInputBox[1].Text, FONT_H1, SPACING_WIDER).x;
                DrawTextEx(GIFont, "|", (Vector2){GoToPageInputBox[1].Box.x + 5 + textWidth, GoToPageInputBox[1].Box.y + 5}, FONT_H1, SPACING_WIDER, BLACK);
            } if (strlen(GoToPageInputBox[1].Text) == 0 && !GoToPageInputBox[1].IsActive) {
                DrawTextEx(GIFont, "01", (Vector2){GoToPageInputBox[1].Box.x + 5, GoToPageInputBox[1].Box.y + 5}, FONT_H1, SPACING_WIDER, Fade(BLACK, 0.6));
            }
        }

        {
            if (CheckCollisionPointRec(Mouse, AFFILIATE_GOTOPAGE_Buttons[5])) {
                HoverTransition[5 + 940] = CustomLerp(HoverTransition[5 + 940], 1.0f, 0.1f);

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {                
                    int RequestedPage = atoi(GoToPageInputBox[1].Text);
                    if (RequestedPage < 1)  { RequestedPage = 1;  }
                    if (RequestedPage > 26) { RequestedPage = 26; }

                    int MaxAvailablePage = (ReadVendors + DISPLAY_PER_PAGE - 1) / DISPLAY_PER_PAGE; // Ensures proper rounding
                    if (RequestedPage > MaxAvailablePage) { RequestedPage = MaxAvailablePage; }

                    snprintf(GoToPageInputBox[1].Text, sizeof(GoToPageInputBox[1].Text), "%02d", RequestedPage);
                    CurrentPages[1] = RequestedPage - 1;

                    PlaySound(ButtonClickSFX);
                    Transitioning = true;
                    ScreenFade = 1.0f;
                    GoingBack = true;
                }
            } else HoverTransition[5 + 940] = CustomLerp(HoverTransition[5 + 940], 0.0f, 0.1f);
            
            AFFILIATE_GOTOPAGE_ButtonColor[5] = (HoverTransition[5 + 940] > 0.5f) ? SKYBLUE : DARKBLUE;
            AFFILIATE_GOTOPAGE_TextColor[5] = (HoverTransition[5 + 940] > 0.5f) ? BLACK : WHITE;
                
            float scale = 1.0f + HoverTransition[5 + 940] * 0.1f;
            float newWidth = AFFILIATE_GOTOPAGE_Buttons[5].width * scale;
            float newHeight = AFFILIATE_GOTOPAGE_Buttons[5].height * scale;
            float newX = AFFILIATE_GOTOPAGE_Buttons[5].x - (newWidth - AFFILIATE_GOTOPAGE_Buttons[5].width) / 2;
            float newY = AFFILIATE_GOTOPAGE_Buttons[5].y - (newHeight - AFFILIATE_GOTOPAGE_Buttons[5].height) / 2;
            
            DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, AFFILIATE_GOTOPAGE_ButtonColor[5]);

            Vector2 textWidth = MeasureTextEx(GIFont, AFFILIATE_GOTOPAGE_ButtonTexts[1], (FONT_H3 * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_H3 * scale / 2);
            DrawTextEx(GIFont, AFFILIATE_GOTOPAGE_ButtonTexts[1], (Vector2){textX, textY}, (FONT_H3 * scale), SPACING_WIDER, AFFILIATE_GOTOPAGE_TextColor[5]);
        }

        for (int i = 0; i < UsersOnPage; i++) {
            int CurrentVendorIndex = i + (CurrentPages[1] * DISPLAY_PER_PAGE);
            
            if (strlen(Vendors[CurrentVendorIndex].AffiliatedSchoolData.Name) == 0) {
                if (CheckCollisionPointRec(Mouse, AFFILIATE_GOTOPAGE_Buttons[i])) {
                    HoverTransition[i + 940] = CustomLerp(HoverTransition[i + 940], 1.0f, 0.1f);

                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        strcpy(GetTemporaryVendorUsername, Vendors[CurrentVendorIndex].Username);
                        GetTemporaryVendorIndex = CurrentVendorIndex;

                        NextMenu = MENU_MAIN_GOVERNMENT_BilateralManagement_CHOOSING;
                        
                        PlaySound(ButtonClickSFX);
                        Transitioning = true;
                        ScreenFade = 1.0f;
                        GoingBack = true;
                    }
                } else {
                    HoverTransition[i + 940] = CustomLerp(HoverTransition[i + 940], 0.0f, 0.1f);
                }

                AFFILIATE_GOTOPAGE_ButtonColor[i] = (HoverTransition[i + 940] > 0.5f) ? GREEN : DARKGREEN;
                AFFILIATE_GOTOPAGE_TextColor[i] = (HoverTransition[i + 940] > 0.5f) ? BLACK : WHITE;

                float scale = 1.0f + HoverTransition[i + 940] * 0.1f;
                float newWidth = AFFILIATE_GOTOPAGE_Buttons[i].width * scale;
                float newHeight = AFFILIATE_GOTOPAGE_Buttons[i].height * scale;
                float newX = AFFILIATE_GOTOPAGE_Buttons[i].x - (newWidth - AFFILIATE_GOTOPAGE_Buttons[i].width) / 2;
                float newY = AFFILIATE_GOTOPAGE_Buttons[i].y - (newHeight - AFFILIATE_GOTOPAGE_Buttons[i].height) / 2;

                DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, AFFILIATE_GOTOPAGE_ButtonColor[i]);

                Vector2 textSize = MeasureTextEx(GIFont, AFFILIATE_GOTOPAGE_ButtonTexts[0], (FONT_H6 * scale), SPACING_WIDER);
                Vector2 textPosition = { newX + (newWidth - textSize.x) / 2, newY + (newHeight - FONT_H6 * scale) / 2 };

                DrawTextEx(GIFont, AFFILIATE_GOTOPAGE_ButtonTexts[0], textPosition, (FONT_H6 * scale), SPACING_WIDER, AFFILIATE_GOTOPAGE_TextColor[i]);
            }
        }

        // Loop for edit and delete buttons
        for (int i = 0; i < UsersOnPage; i++) {
            int CurrentVendorIndex = i + (CurrentPages[1] * DISPLAY_PER_PAGE);

            if (strlen(Vendors[CurrentVendorIndex].AffiliatedSchoolData.Name) > 0) {
                for (int j = 0; j < 2; j++) {  // j = 0 (Edit), j = 1 (Delete)
                    int buttonIndex = (i * 2) + j;
                    
                    if (CheckCollisionPointRec(Mouse, EditDeleteButtons[buttonIndex])) {
                        HoverTransition[buttonIndex + 950] = CustomLerp(HoverTransition[buttonIndex + 950], 1.0f, 0.1f);

                        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                            if (j == 0) {  // Edit
                                strcpy(GetTemporaryVendorUsername, Vendors[CurrentVendorIndex].Username);
                                GetTemporaryVendorIndex = CurrentVendorIndex;
                                NextMenu = MENU_MAIN_GOVERNMENT_BilateralManagement_CHOOSING;
                            } else {  // Delete
                                ReadUserAccount(Vendors);
                                ReadSchoolsData(Schools);

                                // 🔍 Find actual vendor index in OriginalVendors
                                int ActualVendorIndex = -1;
                                for (int j = 0; j < MAX_ACCOUNT_LIMIT; j++) {
                                    if (strcmp(OriginalVendors[j].Username, Vendors[CurrentVendorIndex].Username) == 0) {
                                        ActualVendorIndex = j;
                                        break;
                                    }
                                }

                                if (ActualVendorIndex != -1) { // Ensure vendor is found
                                    // 🔍 Find previous school affiliation
                                    int PreviousSchoolIndex = OriginalVendors[ActualVendorIndex].SaveSchoolDataIndex;

                                    // ❌ Remove affiliation from vendor
                                    strcpy(OriginalVendors[ActualVendorIndex].AffiliatedSchoolData.Name, "");
                                    strcpy(OriginalVendors[ActualVendorIndex].AffiliatedSchoolData.Students, "");
                                    strcpy(OriginalVendors[ActualVendorIndex].AffiliatedSchoolData.AffiliatedVendor, "");
                                    OriginalVendors[ActualVendorIndex].SaveSchoolDataIndex = -1;

                                    // ❌ Remove vendor from the school
                                    if (PreviousSchoolIndex != -1) {
                                        strcpy(OriginalSchools[PreviousSchoolIndex].AffiliatedVendor, "");
                                    }
                                }

                                WriteUserAccount(OriginalVendors);
                                WriteSchoolsData(OriginalSchools);
                                SyncVendorsFromOriginalVendorData(Vendors, OriginalVendors);
                                SyncSchoolsFromOriginalSchoolData(Schools, OriginalSchools);
                                
                                GetTemporaryVendorUsername[0] = '\0';
                                GetTemporaryVendorIndex = -1;

                                NextMenu = MENU_MAIN_GOVERNMENT_BilateralManagement;
                            }

                            PlaySound(ButtonClickSFX);
                            Transitioning = true;
                            ScreenFade = 1.0f;
                            GoingBack = true;
                        }
                    } else {
                        HoverTransition[buttonIndex + 950] = CustomLerp(HoverTransition[buttonIndex + 950], 0.0f, 0.1f);
                    }

                    // Assign colors
                    EditDeleteButtonColor[buttonIndex] = (HoverTransition[buttonIndex + 950] > 0.5f) ? (j == 0 ? YELLOW : PINK) : (j == 0 ? ORANGE : MAROON);
                    EditDeleteTextColor[buttonIndex] = (HoverTransition[buttonIndex + 950] > 0.5f) ? BLACK : WHITE;

                    // Resize button with hover effect
                    float scale = 1.0f + HoverTransition[buttonIndex + 950] * 0.1f;
                    float newWidth = EditDeleteButtons[buttonIndex].width * scale;
                    float newHeight = EditDeleteButtons[buttonIndex].height * scale;
                    float newX = EditDeleteButtons[buttonIndex].x - (newWidth - EditDeleteButtons[buttonIndex].width) / 2;
                    float newY = EditDeleteButtons[buttonIndex].y - (newHeight - EditDeleteButtons[buttonIndex].height) / 2;

                    DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, EditDeleteButtonColor[buttonIndex]);

                    // Draw text
                    Vector2 textSize = MeasureTextEx(GIFont, EditDeleteButtonTexts[j], (FONT_H5 * scale), SPACING_WIDER);
                    Vector2 textPosition = { newX + (newWidth - textSize.x) / 2, newY + (newHeight - FONT_H5 * scale) / 2 };

                    DrawTextEx(GIFont, EditDeleteButtonTexts[j], textPosition, (FONT_H5 * scale), SPACING_WIDER, EditDeleteTextColor[buttonIndex]);
                }
            }
        }

        DrawManageViewBorder((UsersOnPage < 5) ? UsersOnPage : 4);
        for (int i = 0; i < UsersOnPage; i++) {
            snprintf(SAD_No, sizeof(SAD_No), "%03d", (i + 1) + (CurrentPages[1] * DISPLAY_PER_PAGE));
            SAD_Numberings[i].TextFill = SAD_No;
            SAD_Numberings[i].TextPosition = (Vector2){60 + LeftMargin, 240 + (144 * i) + ((TopMargin * 5) / 2) + 3};
            SAD_Numberings[i].FontData = (Vector2){FONT_H2, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_Numberings[i].TextFill, SAD_Numberings[i].TextPosition, SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y, SKYBLUE);

            SAD_FirstLine[i].TextFill = "Nama/Kode Vendor (atau Catering):";
            SAD_FirstLine[i].TextPosition = (Vector2){
                (60 + LeftMargin) + (MeasureTextEx(GIFont, "000", SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y).x + LeftMargin),
                240 + (144 * i) + (TopMargin / 2)
            };
            SAD_FirstLine[i].FontData = (Vector2){FONT_H5, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_FirstLine[i].TextFill, SAD_FirstLine[i].TextPosition, SAD_FirstLine[i].FontData.x, SAD_FirstLine[i].FontData.y, WHITE);

            SAD_VendorName[i].TextFill = Vendors[i].Username;
            SAD_VendorName[i].TextPosition = (Vector2){
                SAD_FirstLine[i].TextPosition.x,
                SAD_FirstLine[i].TextPosition.y + MeasureTextEx(GIFont, SAD_Numberings[i].TextFill, SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y).y - (TopMargin / 3)
            };
            SAD_VendorName[i].FontData = (Vector2){(strlen(SAD_VendorName[i].TextFill) <= 20) ? FONT_H1 : FONT_H2, SPACING_WIDER};
            SAD_VendorName[i].TextPosition.y += (SAD_VendorName[i].FontData.x == FONT_H1) ? 0 : 7;
            DrawTextEx(GIFont, SAD_VendorName[i].TextFill, SAD_VendorName[i].TextPosition, SAD_VendorName[i].FontData.x, SAD_VendorName[i].FontData.y, PURPLE);

            for (int c = 0; c < MAX_INPUT_LENGTH; c++) { SAD_GeneratedSecondLineMeasurement[c] = 'O'; }
            SAD_SecondLine[i].TextFill = "Ter-afiliasi dengan Sekolah:";
            SAD_SecondLine[i].TextPosition = (Vector2){
                (((60 + LeftMargin) + SAD_FirstLine[i].TextPosition.x) * 1.5) + MeasureTextEx(GIFont, SAD_GeneratedSecondLineMeasurement, SAD_FirstLine[i].FontData.x, SAD_FirstLine[i].FontData.y).x,
                240 + (144 * i) + (TopMargin / 2)
            };
            SAD_SecondLine[i].FontData = (Vector2){FONT_H5, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_SecondLine[i].TextFill, SAD_SecondLine[i].TextPosition, SAD_SecondLine[i].FontData.x, SAD_SecondLine[i].FontData.y, WHITE);
            SAD_GeneratedSecondLineMeasurement[0] = '\0';

            if (strlen(Vendors[i].AffiliatedSchoolData.Name) == 0) SAD_SchoolName[i].TextFill = "-";
            else SAD_SchoolName[i].TextFill = Vendors[i].AffiliatedSchoolData.Name;
            SAD_SchoolName[i].TextPosition = (Vector2){
                SAD_SecondLine[i].TextPosition.x,
                SAD_SecondLine[i].TextPosition.y + MeasureTextEx(GIFont, SAD_SecondLine[i].TextFill, SAD_SecondLine[i].FontData.x, SAD_SecondLine[i].FontData.y).y + (TopMargin + 5)
            };
            SAD_SchoolName[i].FontData = (Vector2){(strlen(SAD_SchoolName[i].TextFill) <= 20) ? FONT_H1 : FONT_H2, SPACING_WIDER};
            SAD_SchoolName[i].TextPosition.y -= (SAD_SchoolName[i].FontData.x == FONT_H1) ? 12 : 6;
            DrawTextEx(GIFont, SAD_SchoolName[i].TextFill, SAD_SchoolName[i].TextPosition, SAD_SchoolName[i].FontData.x, SAD_SchoolName[i].FontData.y, YELLOW);

            SAD_ThirdLine[i].TextFill = "Banyak Murid:";
            SAD_ThirdLine[i].TextPosition = (Vector2){
                SAD_SecondLine[i].TextPosition.x,
                (278 + (144 * i) + TopMargin) + MeasureTextEx(GIFont, SAD_SchoolName[i].TextFill, SAD_SchoolName[i].FontData.x, SAD_SchoolName[i].FontData.y).y + (SAD_SchoolName[i].FontData.x == FONT_H1 ? 0 : 12)
            };
            SAD_ThirdLine[i].FontData = (Vector2){FONT_H5, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_ThirdLine[i].TextFill, SAD_ThirdLine[i].TextPosition, SAD_ThirdLine[i].FontData.x, SAD_ThirdLine[i].FontData.y, WHITE);

            if (strlen(Vendors[i].AffiliatedSchoolData.Students) == 0) SAD_ManyStudents[i].TextFill = "...";
            else SAD_ManyStudents[i].TextFill = Vendors[i].AffiliatedSchoolData.Students;
            SAD_ManyStudents[i].TextPosition = (Vector2){
                SAD_ThirdLine[i].TextPosition.x + MeasureTextEx(GIFont, SAD_ThirdLine[i].TextFill, SAD_ThirdLine[i].FontData.x, SAD_ThirdLine[i].FontData.y).x + 10,
                SAD_ThirdLine[i].TextPosition.y - 2.38
            };
            SAD_ManyStudents[i].FontData = (Vector2){FONT_H4, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_ManyStudents[i].TextFill, SAD_ManyStudents[i].TextPosition, SAD_ManyStudents[i].FontData.x, SAD_ManyStudents[i].FontData.y, BEIGE);
        }

        if (IsKeyPressed(KEY_RIGHT) && UsersOnPage == DISPLAY_PER_PAGE && (CurrentPages[1] * DISPLAY_PER_PAGE) + UsersOnPage < ReadVendors) { CurrentPages[1]++; }
        if (IsKeyPressed(KEY_LEFT) && CurrentPages[1] > 0) { CurrentPages[1]--; }
        
        DrawTextEx(GIFont, ShowRegisteredSchools.TextFill, ShowRegisteredSchools.TextPosition, ShowRegisteredSchools.FontData.x, ShowRegisteredSchools.FontData.y, PINK);
        DrawTextEx(GIFont, AffiliationMessage.TextFill, AffiliationMessage.TextPosition, AffiliationMessage.FontData.x, AffiliationMessage.FontData.y, ORANGE);
        DrawTextEx(GIFont, GoToPageGuide.TextFill, GoToPageGuide.TextPosition, GoToPageGuide.FontData.x, GoToPageGuide.FontData.y, WHITE);
        DrawTextEx(GIFont, PaginationInfo.TextFill, PaginationInfo.TextPosition, PaginationInfo.FontData.x, PaginationInfo.FontData.y, WHITE);
        DrawTextEx(GIFont, PaginationGuide.TextFill, PaginationGuide.TextPosition, PaginationGuide.FontData.x, PaginationGuide.FontData.y, GREEN);
    }
}

void GovernmentManageBilateralsMenu_CHOOSING(void) {
    char ReturnMenuState[256] = { 0 };
    int LeftMargin = 40, TopMargin = 20;
    
    Rectangle AFFILIATE_GOTOPAGE_Buttons[6] = {
        {SCREEN_WIDTH - 240 - 15, 240 + 48, 180, 50}, 
        {AFFILIATE_GOTOPAGE_Buttons[0].x, AFFILIATE_GOTOPAGE_Buttons[0].y + 144, 180, 50}, 
        {AFFILIATE_GOTOPAGE_Buttons[1].x, AFFILIATE_GOTOPAGE_Buttons[1].y + 144, 180, 50}, 
        {AFFILIATE_GOTOPAGE_Buttons[2].x, AFFILIATE_GOTOPAGE_Buttons[2].y + 144, 180, 50}, 
        {AFFILIATE_GOTOPAGE_Buttons[3].x, AFFILIATE_GOTOPAGE_Buttons[3].y + 144, 180, 50}, 
        {SCREEN_WIDTH - 180, 160, 120, 60}
    };
    Color AFFILIATE_GOTOPAGE_ButtonColor[6], AFFILIATE_GOTOPAGE_TextColor[6];
    const char *AFFILIATE_GOTOPAGE_ButtonTexts[] = {"[+] Pasangkan...", "Tuju!"};

    const char *SortingSettingButtonText = "[@] Urutkan...";
    Rectangle SortingSettingButton = {60, AFFILIATE_GOTOPAGE_Buttons[5].y - 30, (AFFILIATE_GOTOPAGE_Buttons[5].width * 2), AFFILIATE_GOTOPAGE_Buttons[5].height};
    Color SortingSettingButtonColor, SortingSettingTextColor;

    PreviousMenu = MENU_MAIN_GOVERNMENT_BilateralManagement;
    DrawPreviousMenuButton();

    GetMenuState(CurrentMenu, ReturnMenuState, false);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H2, SPACING_WIDER, (Vector2){0, 60}, 0, true);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

    const char *DataTexts[] = {
        "Mohon maaf, saat ini belum ada data Sekolah yang masih belum ter-afiliasikan oleh Anda.",
        "Anda dipersilakan untuk menambahkan data Sekolah baru (beserta banyak siswa-nya) pada menu `[MANAGE]: Data Sekolah` sebagai bahan data afiliasi terbaru.",
        "Terima kasih atas kerja samanya, Pemerintah setempat..."
    };
    TextObject NoSchoolsMessage[3] = {
        CreateTextObject(GIFont, DataTexts[0], FONT_H5, SPACING_WIDER, (Vector2){0, (SCREEN_HEIGHT / 2) - 30}, 0, true),
        CreateTextObject(GIFont, DataTexts[1], FONT_H5, SPACING_WIDER, (Vector2){0, NoSchoolsMessage[0].TextPosition.y}, 40, true),
        CreateTextObject(GIFont, DataTexts[2], FONT_H3, SPACING_WIDER, (Vector2){0, NoSchoolsMessage[1].TextPosition.y}, 80, true),
    };

    char SAD_No[16] = { 0 };
    TextObject SAD_Numberings[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_SchoolName[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_SecondLine[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_ThirdLine[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_ManyStudents[MAX_ACCOUNT_LIMIT] = { 0 };
    
    GoToPageInputBox[2].Box = (Rectangle){AFFILIATE_GOTOPAGE_Buttons[5].x - AFFILIATE_GOTOPAGE_Buttons[5].width + 20, AFFILIATE_GOTOPAGE_Buttons[5].y, 80, 60};

    char ShowRegisteredSchoolsText[128] = { 0 };
    const char *PrepareGoToPageGuide = "Pergi ke halaman:";
    const char *PreparePaginationGuide = "Tekan [<] atau [>] di keyboard laptop/PC Anda untuk bernavigasi...";
    char PrepareAffiliationMessage[256] = { 0 };
    char PreparePaginationTextInfo[64] = { 0 };

    snprintf(ShowRegisteredSchoolsText, sizeof(ShowRegisteredSchoolsText), "Silakan pilih target afiliasi Vendor: \"%s\" dengan Sekolah yang Anda inginkan...", GetTemporaryVendorUsername);
    TextObject ShowRegisteredSchools = CreateTextObject(GIFont, ShowRegisteredSchoolsText, (strlen(GetTemporaryVendorUsername) <= 20) ? FONT_P : FONT_H5, SPACING_WIDER, (Vector2){60, 200}, 0, false);
    snprintf(PrepareAffiliationMessage, sizeof(PrepareAffiliationMessage), "Terdapat [%d] data Sekolah yang masih BELUM ter-afiliasikan.\nMohon untuk pilih SALAH SATU dari beberapa target Sekolah yang tersedia...?", ReadOnlyNonAffiliatedSchools);
    TextObject AffiliationMessage = CreateTextObject(GIFont, PrepareAffiliationMessage, FONT_H5, SPACING_WIDER, (Vector2){60, 980}, 0, false);
    snprintf(PreparePaginationTextInfo, sizeof(PreparePaginationTextInfo), "Halaman %d dari %d", (CurrentPages[2] + 1), (ReadOnlyNonAffiliatedSchools % DISPLAY_PER_PAGE != 0) ? ((ReadOnlyNonAffiliatedSchools / DISPLAY_PER_PAGE) + 1) : (ReadOnlyNonAffiliatedSchools / DISPLAY_PER_PAGE));
    TextObject GoToPageGuide = CreateTextObject(GIFont, PrepareGoToPageGuide, FONT_H5, SPACING_WIDER, (Vector2){GoToPageInputBox[2].Box.x, GoToPageInputBox[2].Box.y - (GoToPageInputBox[2].Box.height / 2)}, 0, false);
    TextObject PaginationInfo = CreateTextObject(GIFont, PreparePaginationTextInfo, FONT_P, SPACING_WIDER, (Vector2){SCREEN_WIDTH - 60 - MeasureTextEx(GIFont, PreparePaginationTextInfo, FONT_P, SPACING_WIDER).x, 980}, 0, false);
    TextObject PaginationGuide = CreateTextObject(GIFont, PreparePaginationGuide, FONT_H5, SPACING_WIDER, (Vector2){SCREEN_WIDTH - 60 - MeasureTextEx(GIFont, PreparePaginationGuide, FONT_H5, SPACING_WIDER).x, PaginationInfo.TextPosition.y}, 40, false);

    // UsersOnPage = ReadUsersWithPagination(CurrentPages[2], Vendors);
    ReadOnlyNonAffiliatedSchools = ReadNonAffiliatedSchools(OnlyNonAffiliatedSchools);
    OnlyNonAffiliatedSchoolsOnPage = ReadNonAffiliatedSchoolsWithPagination(CurrentPages[2], OnlyNonAffiliatedSchools);
    
    Vector2 Mouse = GetMousePosition();
    if (ReadOnlyNonAffiliatedSchools == 0) {
        // DrawManageViewBorder(0);
        for (int i = 0; i < 3; i++) {
            DrawTextEx(GIFont, NoSchoolsMessage[i].TextFill, NoSchoolsMessage[i].TextPosition, NoSchoolsMessage[i].FontData.x, NoSchoolsMessage[i].FontData.y, (i != 2) ? YELLOW : WHITE);
        }
    } else {
        {
            if (CheckCollisionPointRec(Mouse, SortingSettingButton)) {
                HoverTransition[(5 + 940) + 1] = CustomLerp(HoverTransition[(5 + 940) + 1], 1.0f, 0.1f);

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {                
                    NextMenu = MENU_SUBMAIN_SortingOptions;
                    PreviousMenu = CurrentMenu;

                    PlaySound(ButtonClickSFX);
                    Transitioning = true;
                    ScreenFade = 1.0f;
                    GoingBack = true;
                }
            } else HoverTransition[(5 + 940) + 1] = CustomLerp(HoverTransition[(5 + 940) + 1], 0.0f, 0.1f);
            
            SortingSettingButtonColor = (HoverTransition[(5 + 940) + 1] > 0.5f) ? PURPLE : DARKPURPLE;
            SortingSettingTextColor = (HoverTransition[(5 + 940) + 1] > 0.5f) ? BLACK : WHITE;
                
            float scale = 1.0f + HoverTransition[(5 + 940) + 1] * 0.1f;
            float newWidth = SortingSettingButton.width * scale;
            float newHeight = SortingSettingButton.height * scale;
            float newX = SortingSettingButton.x - (newWidth - SortingSettingButton.width) / 2;
            float newY = SortingSettingButton.y - (newHeight - SortingSettingButton.height) / 2;
            
            DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, SortingSettingButtonColor);

            Vector2 textWidth = MeasureTextEx(GIFont, SortingSettingButtonText, (FONT_H3 * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_H3 * scale / 2);
            DrawTextEx(GIFont, SortingSettingButtonText, (Vector2){textX, textY}, (FONT_H3 * scale), SPACING_WIDER, SortingSettingTextColor);
        }

        {
            DrawRectangleRec(GoToPageInputBox[2].Box, GRAY);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (CheckCollisionPointRec(Mouse, GoToPageInputBox[2].Box)) { GoToPageInputBox[2].IsActive = true; }
                else { GoToPageInputBox[2].IsActive = false; }
            }

            if (GoToPageInputBox[2].IsActive) {
                int Key = GetCharPressed();
                while (Key >= '0' && Key <= '9') {
                    if (strlen(GoToPageInputBox[2].Text) < 2) {
                        int len = strlen(GoToPageInputBox[2].Text);
                        GoToPageInputBox[2].Text[len] = (char)Key;
                        GoToPageInputBox[2].Text[len + 1] = '\0';
                    } Key = GetCharPressed();
                }

                if (IsKeyPressed(KEY_BACKSPACE) && strlen(GoToPageInputBox[2].Text) > 0) {
                    GoToPageInputBox[2].Text[strlen(GoToPageInputBox[2].Text) - 1] = '\0';
                }
            }

            FrameCounter++; // Increment frame counter for cursor blinking
            Color boxColor = GRAY;    
            if (GoToPageInputBox[2].IsActive) boxColor = SKYBLUE; // Active input
            else if (CheckCollisionPointRec(Mouse, GoToPageInputBox[2].Box)) boxColor = YELLOW; // Hover effect

            DrawRectangleRec(GoToPageInputBox[2].Box, boxColor);
            DrawTextEx(GIFont, GoToPageInputBox[2].Text, (Vector2){GoToPageInputBox[2].Box.x + 5, GoToPageInputBox[2].Box.y + 5}, FONT_H1, SPACING_WIDER, BLACK);

            if (GoToPageInputBox[2].IsActive && (FrameCounter / CURSOR_BLINK_SPEED) % 2 == 0) {
                int textWidth = MeasureTextEx(GIFont, GoToPageInputBox[2].Text, FONT_H1, SPACING_WIDER).x;
                DrawTextEx(GIFont, "|", (Vector2){GoToPageInputBox[2].Box.x + 5 + textWidth, GoToPageInputBox[2].Box.y + 5}, FONT_H1, SPACING_WIDER, BLACK);
            } if (strlen(GoToPageInputBox[2].Text) == 0 && !GoToPageInputBox[2].IsActive) {
                DrawTextEx(GIFont, "01", (Vector2){GoToPageInputBox[2].Box.x + 5, GoToPageInputBox[2].Box.y + 5}, FONT_H1, SPACING_WIDER, Fade(BLACK, 0.6));
            }
        }

        {
            if (CheckCollisionPointRec(Mouse, AFFILIATE_GOTOPAGE_Buttons[5])) {
                HoverTransition[5 + 940] = CustomLerp(HoverTransition[5 + 940], 1.0f, 0.1f);

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {                
                    int RequestedPage = atoi(GoToPageInputBox[2].Text);
                    if (RequestedPage < 1)  { RequestedPage = 1;  }
                    if (RequestedPage > 26) { RequestedPage = 26; }

                    int MaxAvailablePage = (ReadOnlyNonAffiliatedSchools + DISPLAY_PER_PAGE - 1) / DISPLAY_PER_PAGE; // Ensures proper rounding
                    if (RequestedPage > MaxAvailablePage) { RequestedPage = MaxAvailablePage; }

                    snprintf(GoToPageInputBox[2].Text, sizeof(GoToPageInputBox[2].Text), "%02d", RequestedPage);
                    CurrentPages[2] = RequestedPage - 1;

                    PlaySound(ButtonClickSFX);
                    Transitioning = true;
                    ScreenFade = 1.0f;
                    GoingBack = true;
                }
            } else HoverTransition[5 + 940] = CustomLerp(HoverTransition[5 + 940], 0.0f, 0.1f);
            
            AFFILIATE_GOTOPAGE_ButtonColor[5] = (HoverTransition[5 + 940] > 0.5f) ? SKYBLUE : DARKBLUE;
            AFFILIATE_GOTOPAGE_TextColor[5] = (HoverTransition[5 + 940] > 0.5f) ? BLACK : WHITE;
                
            float scale = 1.0f + HoverTransition[5 + 940] * 0.1f;
            float newWidth = AFFILIATE_GOTOPAGE_Buttons[5].width * scale;
            float newHeight = AFFILIATE_GOTOPAGE_Buttons[5].height * scale;
            float newX = AFFILIATE_GOTOPAGE_Buttons[5].x - (newWidth - AFFILIATE_GOTOPAGE_Buttons[5].width) / 2;
            float newY = AFFILIATE_GOTOPAGE_Buttons[5].y - (newHeight - AFFILIATE_GOTOPAGE_Buttons[5].height) / 2;
            
            DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, AFFILIATE_GOTOPAGE_ButtonColor[5]);

            Vector2 textWidth = MeasureTextEx(GIFont, AFFILIATE_GOTOPAGE_ButtonTexts[1], (FONT_H3 * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_H3 * scale / 2);
            DrawTextEx(GIFont, AFFILIATE_GOTOPAGE_ButtonTexts[1], (Vector2){textX, textY}, (FONT_H3 * scale), SPACING_WIDER, AFFILIATE_GOTOPAGE_TextColor[5]);
        }

        for (int i = 0; i < OnlyNonAffiliatedSchoolsOnPage; i++) {
            int ActualSchoolListIndex = i + (CurrentPages[2] * DISPLAY_PER_PAGE); // ✅ Convert page index to full index
        
            if (CheckCollisionPointRec(Mouse, AFFILIATE_GOTOPAGE_Buttons[i])) {
                HoverTransition[i + 940] = CustomLerp(HoverTransition[i + 940], 1.0f, 0.1f);
        
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    ReadUserAccount(Vendors);
                    ReadSchoolsData(Schools);
        
                    // 🔍 Find actual school index in OriginalSchools
                    int ActualSchoolIndex = -1;
                    for (int j = 0; j < MAX_ACCOUNT_LIMIT; j++) {
                        if (strcmp(OriginalSchools[j].Name, OnlyNonAffiliatedSchools[ActualSchoolListIndex].Name) == 0) {
                            ActualSchoolIndex = j;
                            break;
                        }
                    }
        
                    // 🔍 Find actual vendor index in OriginalVendors
                    int ActualVendorIndex = -1;
                    for (int j = 0; j < MAX_ACCOUNT_LIMIT; j++) {
                        if (strcmp(OriginalVendors[j].Username, Vendors[GetTemporaryVendorIndex].Username) == 0) {
                            ActualVendorIndex = j;
                            break;
                        }
                    }
        
                    if (ActualVendorIndex != -1) { // Ensure vendor is found
                        // ❌ Remove old affiliation
                        strcpy(OriginalVendors[ActualVendorIndex].AffiliatedSchoolData.Name, "");
                        strcpy(OriginalVendors[ActualVendorIndex].AffiliatedSchoolData.Students, "");
                        strcpy(OriginalVendors[ActualVendorIndex].AffiliatedSchoolData.AffiliatedVendor, "");
        
                        int PreviousSchoolIndex = OriginalVendors[ActualVendorIndex].SaveSchoolDataIndex;
                        if (PreviousSchoolIndex != -1) {
                            strcpy(OriginalSchools[PreviousSchoolIndex].AffiliatedVendor, "");
                        }
                        OriginalVendors[ActualVendorIndex].SaveSchoolDataIndex = -1;
        
                        // ✅ Apply new affiliation
                        strcpy(OriginalVendors[ActualVendorIndex].AffiliatedSchoolData.Name, OnlyNonAffiliatedSchools[ActualSchoolListIndex].Name);
                        strcpy(OriginalVendors[ActualVendorIndex].AffiliatedSchoolData.Students, OnlyNonAffiliatedSchools[ActualSchoolListIndex].Students);
                        strcpy(OriginalVendors[ActualVendorIndex].AffiliatedSchoolData.AffiliatedVendor, GetTemporaryVendorUsername);
        
                        if (ActualSchoolIndex != -1) {
                            strcpy(OriginalSchools[ActualSchoolIndex].AffiliatedVendor, GetTemporaryVendorUsername);
                            OriginalVendors[ActualVendorIndex].SaveSchoolDataIndex = ActualSchoolIndex;
                        }
                    }
                    
                    WriteUserAccount(OriginalVendors);
                    WriteSchoolsData(OriginalSchools);
                    SyncVendorsFromOriginalVendorData(Vendors, OriginalVendors);
                    SyncSchoolsFromOriginalSchoolData(Schools, OriginalSchools);
        
                    NextMenu = MENU_MAIN_GOVERNMENT_BilateralManagement;

                    PlaySound(ButtonClickSFX);
                    Transitioning = true;
                    ScreenFade = 1.0f;
                    GoingBack = true;
                }
            } else {
                HoverTransition[i + 940] = CustomLerp(HoverTransition[i + 940], 0.0f, 0.1f);
            }
        
            // 🎨 Button effects
            AFFILIATE_GOTOPAGE_ButtonColor[i] = (HoverTransition[i + 940] > 0.5f) ? SKYBLUE : DARKBLUE;
            AFFILIATE_GOTOPAGE_TextColor[i] = (HoverTransition[i + 940] > 0.5f) ? BLACK : WHITE;
            
            float scale = 1.0f + HoverTransition[i + 940] * 0.1f;
            float newWidth = AFFILIATE_GOTOPAGE_Buttons[i].width * scale;
            float newHeight = AFFILIATE_GOTOPAGE_Buttons[i].height * scale;
            float newX = AFFILIATE_GOTOPAGE_Buttons[i].x - (newWidth - AFFILIATE_GOTOPAGE_Buttons[i].width) / 2;
            float newY = AFFILIATE_GOTOPAGE_Buttons[i].y - (newHeight - AFFILIATE_GOTOPAGE_Buttons[i].height) / 2;
            
            DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, AFFILIATE_GOTOPAGE_ButtonColor[i]);
        
            Vector2 textWidth = MeasureTextEx(GIFont, AFFILIATE_GOTOPAGE_ButtonTexts[0], (FONT_H6 * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_H6 * scale / 2);
            DrawTextEx(GIFont, AFFILIATE_GOTOPAGE_ButtonTexts[0], (Vector2){textX, textY}, (FONT_H6 * scale), SPACING_WIDER, AFFILIATE_GOTOPAGE_TextColor[i]);
        }        

        DrawManageViewBorder((OnlyNonAffiliatedSchoolsOnPage < 5) ? OnlyNonAffiliatedSchoolsOnPage : 4);
        for (int i = 0; i < OnlyNonAffiliatedSchoolsOnPage; i++) {
            snprintf(SAD_No, sizeof(SAD_No), "%03d", (i + 1) + (CurrentPages[2] * DISPLAY_PER_PAGE));
            SAD_Numberings[i].TextFill = SAD_No;
            SAD_Numberings[i].TextPosition = (Vector2){60 + LeftMargin, 240 + (144 * i) + ((TopMargin * 5) / 2) + 3};
            SAD_Numberings[i].FontData = (Vector2){FONT_H2, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_Numberings[i].TextFill, SAD_Numberings[i].TextPosition, SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y, SKYBLUE);

            // SAD_FirstLine[i].TextFill = "Nama/Kode Vendor (atau Catering):";
            // SAD_FirstLine[i].TextPosition = (Vector2){
            //     (60 + LeftMargin) + (MeasureTextEx(GIFont, "000", SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y).x + LeftMargin),
            //     240 + (144 * i) + (TopMargin / 2)
            // };
            // SAD_FirstLine[i].FontData = (Vector2){FONT_H5, SPACING_WIDER};
            // DrawTextEx(GIFont, SAD_FirstLine[i].TextFill, SAD_FirstLine[i].TextPosition, SAD_FirstLine[i].FontData.x, SAD_FirstLine[i].FontData.y, WHITE);

            // SAD_VendorName[i].TextFill = Vendors[i].Username;
            // SAD_VendorName[i].TextPosition = (Vector2){
            //     SAD_FirstLine[i].TextPosition.x,
            //     SAD_FirstLine[i].TextPosition.y + MeasureTextEx(GIFont, SAD_Numberings[i].TextFill, SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y).y - (TopMargin / 3)
            // };
            // SAD_VendorName[i].FontData = (Vector2){(strlen(SAD_VendorName[i].TextFill) <= 20) ? FONT_H1 : FONT_H2, SPACING_WIDER};
            // SAD_VendorName[i].TextPosition.y += (SAD_VendorName[i].FontData.x == FONT_H1) ? 0 : 7;
            // DrawTextEx(GIFont, SAD_VendorName[i].TextFill, SAD_VendorName[i].TextPosition, SAD_VendorName[i].FontData.x, SAD_VendorName[i].FontData.y, PURPLE);

            SAD_SecondLine[i].TextFill = "Nama Sekolah:";
            SAD_SecondLine[i].TextPosition = (Vector2){
                (60 + LeftMargin) + (MeasureTextEx(GIFont, "000", SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y).x + LeftMargin),
                240 + (144 * i) + (TopMargin / 2)
            };
            SAD_SecondLine[i].FontData = (Vector2){FONT_H5, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_SecondLine[i].TextFill, SAD_SecondLine[i].TextPosition, SAD_SecondLine[i].FontData.x, SAD_SecondLine[i].FontData.y, WHITE);

            // if (strlen(Vendors[i].AffiliatedSchoolData.Name) == 0) SAD_SchoolName[i].TextFill = "-";
            // else SAD_SchoolName[i].TextFill = Vendors[i].AffiliatedSchoolData.Name;
            SAD_SchoolName[i].TextFill = OnlyNonAffiliatedSchools[i].Name;
            SAD_SchoolName[i].TextPosition = (Vector2){
                SAD_SecondLine[i].TextPosition.x,
                SAD_SecondLine[i].TextPosition.y + MeasureTextEx(GIFont, SAD_SecondLine[i].TextFill, SAD_SecondLine[i].FontData.x, SAD_SecondLine[i].FontData.y).y + (TopMargin + 5)
            };
            SAD_SchoolName[i].FontData = (Vector2){(strlen(SAD_SchoolName[i].TextFill) <= 20) ? FONT_H1 : FONT_H2, SPACING_WIDER};
            SAD_SchoolName[i].TextPosition.y -= (SAD_SchoolName[i].FontData.x == FONT_H1) ? 12 : 6;
            DrawTextEx(GIFont, SAD_SchoolName[i].TextFill, SAD_SchoolName[i].TextPosition, SAD_SchoolName[i].FontData.x, SAD_SchoolName[i].FontData.y, YELLOW);

            SAD_ThirdLine[i].TextFill = "Banyak Murid:";
            SAD_ThirdLine[i].TextPosition = (Vector2){
                SAD_SecondLine[i].TextPosition.x,
                (278 + (144 * i) + TopMargin) + MeasureTextEx(GIFont, SAD_SchoolName[i].TextFill, SAD_SchoolName[i].FontData.x, SAD_SchoolName[i].FontData.y).y + (SAD_SchoolName[i].FontData.x == FONT_H1 ? 0 : 12)
            };
            SAD_ThirdLine[i].FontData = (Vector2){FONT_H5, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_ThirdLine[i].TextFill, SAD_ThirdLine[i].TextPosition, SAD_ThirdLine[i].FontData.x, SAD_ThirdLine[i].FontData.y, WHITE);

            SAD_ManyStudents[i].TextFill = OnlyNonAffiliatedSchools[i].Students;
            SAD_ManyStudents[i].TextPosition = (Vector2){
                SAD_ThirdLine[i].TextPosition.x + MeasureTextEx(GIFont, SAD_ThirdLine[i].TextFill, SAD_ThirdLine[i].FontData.x, SAD_ThirdLine[i].FontData.y).x + 10,
                SAD_ThirdLine[i].TextPosition.y - 2.38
            };
            SAD_ManyStudents[i].FontData = (Vector2){FONT_H4, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_ManyStudents[i].TextFill, SAD_ManyStudents[i].TextPosition, SAD_ManyStudents[i].FontData.x, SAD_ManyStudents[i].FontData.y, BEIGE);
        }

        if (IsKeyPressed(KEY_RIGHT) && OnlyNonAffiliatedSchoolsOnPage == DISPLAY_PER_PAGE && (CurrentPages[2] * DISPLAY_PER_PAGE) + OnlyNonAffiliatedSchoolsOnPage < ReadOnlyNonAffiliatedSchools) { CurrentPages[2]++; }
        if (IsKeyPressed(KEY_LEFT) && CurrentPages[2] > 0) { CurrentPages[2]--; }
        
        DrawTextEx(GIFont, ShowRegisteredSchools.TextFill, ShowRegisteredSchools.TextPosition, ShowRegisteredSchools.FontData.x, ShowRegisteredSchools.FontData.y, PINK);
        DrawTextEx(GIFont, AffiliationMessage.TextFill, AffiliationMessage.TextPosition, AffiliationMessage.FontData.x, AffiliationMessage.FontData.y, ORANGE);
        DrawTextEx(GIFont, GoToPageGuide.TextFill, GoToPageGuide.TextPosition, GoToPageGuide.FontData.x, GoToPageGuide.FontData.y, WHITE);
        DrawTextEx(GIFont, PaginationInfo.TextFill, PaginationInfo.TextPosition, PaginationInfo.FontData.x, PaginationInfo.FontData.y, WHITE);
        DrawTextEx(GIFont, PaginationGuide.TextFill, PaginationGuide.TextPosition, PaginationGuide.FontData.x, PaginationGuide.FontData.y, GREEN);
    }
}

void GovernmentViewVendorsMenu(void) {
    char ReturnMenuState[256] = { 0 };
    int LeftMargin = 40, TopMargin = 20;
    Vector2 ButtonSize = {180, 50};
    
    Rectangle SHOWDETAILS_GOTOPAGE_Buttons[6] = {
        {SCREEN_WIDTH - 240 - 15, 240 + 48, ButtonSize.x, ButtonSize.y}, 
        {SHOWDETAILS_GOTOPAGE_Buttons[0].x, SHOWDETAILS_GOTOPAGE_Buttons[0].y + 144, ButtonSize.x, ButtonSize.y}, 
        {SHOWDETAILS_GOTOPAGE_Buttons[1].x, SHOWDETAILS_GOTOPAGE_Buttons[1].y + 144, ButtonSize.x, ButtonSize.y}, 
        {SHOWDETAILS_GOTOPAGE_Buttons[2].x, SHOWDETAILS_GOTOPAGE_Buttons[2].y + 144, ButtonSize.x, ButtonSize.y}, 
        {SHOWDETAILS_GOTOPAGE_Buttons[3].x, SHOWDETAILS_GOTOPAGE_Buttons[3].y + 144, ButtonSize.x, ButtonSize.y}, 
        {SCREEN_WIDTH - 180, 160, 120, 60}
    };
    Color SHOWDETAILS_GOTOPAGE_ButtonColor[6], SHOWDETAILS_GOTOPAGE_TextColor[6];
    const char *SHOWDETAILS_GOTOPAGE_ButtonTexts[] = {"[>] Detail...", "Tuju!"};

    const char *SortingSettingButtonText = "[@] Urutkan...";
    Rectangle SortingSettingButton = {60, SHOWDETAILS_GOTOPAGE_Buttons[5].y, (SHOWDETAILS_GOTOPAGE_Buttons[5].width * 2), SHOWDETAILS_GOTOPAGE_Buttons[5].height};
    Color SortingSettingButtonColor, SortingSettingTextColor;

    PreviousMenu = MENU_MAIN_GOVERNMENT;
    DrawPreviousMenuButton();

    GetMenuState(CurrentMenu, ReturnMenuState, false);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H2, SPACING_WIDER, (Vector2){0, 60}, 0, true);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

    const char *DataTexts[] = {
        "Mohon maaf, saat ini belum ada pihak Vendor (atau Catering) yang telah mendaftarkan diri.",
        "Harap untuk menunggu sampai pada pembaruan berikutnya apabila sudah ada Vendor yang telah melakukan registrasi di D'Magis ini.",
        "Terima kasih atas perhatiannya, Pemerintah setempat..."
    };
    TextObject NoVendorsMessage[3] = {
        CreateTextObject(GIFont, DataTexts[0], FONT_H5, SPACING_WIDER, (Vector2){0, (SCREEN_HEIGHT / 2) - 30}, 0, true),
        CreateTextObject(GIFont, DataTexts[1], FONT_H5, SPACING_WIDER, (Vector2){0, NoVendorsMessage[0].TextPosition.y}, 40, true),
        CreateTextObject(GIFont, DataTexts[2], FONT_H3, SPACING_WIDER, (Vector2){0, NoVendorsMessage[1].TextPosition.y}, 80, true),
    };

    char SAD_No[16] = { 0 }, SAD_GeneratedSecondLineMeasurement[32] = { 0 };
    TextObject SAD_Numberings[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_FirstLine[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_SchoolName[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_SecondLine[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_VendorName[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_ThirdLine[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_ManyStudents[MAX_ACCOUNT_LIMIT] = { 0 };
    
    GoToPageInputBox[3].Box = (Rectangle){SHOWDETAILS_GOTOPAGE_Buttons[5].x - SHOWDETAILS_GOTOPAGE_Buttons[5].width + 20, SHOWDETAILS_GOTOPAGE_Buttons[5].y, 80, 60};

    char ShowRegisteredVendorsText[128] = { 0 };
    snprintf(ShowRegisteredVendorsText, sizeof(ShowRegisteredVendorsText), "Sebanyak `%d dari %d` akun Vendor (atau Catering) telah terdata...", ReadVendors, MAX_ACCOUNT_LIMIT);
    TextObject ShowRegisteredVendors = CreateTextObject(GIFont, ShowRegisteredVendorsText, FONT_P, SPACING_WIDER, (Vector2){0, 200}, 0, true);
    char PreparePaginationTextInfo[64] = { 0 };
    const char *PrepareGoToPageGuide = "Pergi ke halaman:";
    const char *PreparePaginationGuide = "Tekan [<] atau [>] di keyboard laptop/PC Anda untuk bernavigasi...";
    snprintf(PreparePaginationTextInfo, sizeof(PreparePaginationTextInfo), "Halaman %d dari %d", (CurrentPages[3] + 1), ((ReadVendors % DISPLAY_PER_PAGE) != 0) ? ((ReadVendors / DISPLAY_PER_PAGE) + 1) : (ReadVendors / DISPLAY_PER_PAGE));
    TextObject GoToPageGuide = CreateTextObject(GIFont, PrepareGoToPageGuide, FONT_H5, SPACING_WIDER, (Vector2){GoToPageInputBox[3].Box.x, GoToPageInputBox[3].Box.y - (GoToPageInputBox[3].Box.height / 2)}, 0, false);
    TextObject PaginationInfo = CreateTextObject(GIFont, PreparePaginationTextInfo, FONT_P, SPACING_WIDER, (Vector2){SCREEN_WIDTH - 60 - MeasureTextEx(GIFont, PreparePaginationTextInfo, FONT_P, SPACING_WIDER).x, 980}, 0, false);
    TextObject PaginationGuide = CreateTextObject(GIFont, PreparePaginationGuide, FONT_H5, SPACING_WIDER, (Vector2){SCREEN_WIDTH - 60 - MeasureTextEx(GIFont, PreparePaginationGuide, FONT_H5, SPACING_WIDER).x, PaginationInfo.TextPosition.y}, 40, false);

    ReadVendors = ReadUserAccount(Vendors);
    UsersOnPage = ReadUsersWithPagination(CurrentPages[3], Vendors);

    // Always reset the menu profile cards...
    IsCardTransitioning = false;
    CurrentCardMenuProfile = 0;
    SetCardPositionInX = 0; // Start off-screen left
    TargetX = 0;
    TransitionDirection = 0; // 1 = right, -1 = left
    // Always reset the menu profile cards...

    Vector2 Mouse = GetMousePosition();
    if (ReadVendors == 0) {
        // DrawManageViewBorder(0);
        for (int i = 0; i < 3; i++) {
            DrawTextEx(GIFont, NoVendorsMessage[i].TextFill, NoVendorsMessage[i].TextPosition, NoVendorsMessage[i].FontData.x, NoVendorsMessage[i].FontData.y, (i != 2) ? YELLOW : PINK);
        }
    } else {
        {
            if (CheckCollisionPointRec(Mouse, SortingSettingButton)) {
                HoverTransition[(5 + 940) + 1] = CustomLerp(HoverTransition[(5 + 940) + 1], 1.0f, 0.1f);

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {                
                    NextMenu = MENU_SUBMAIN_SortingOptions;
                    PreviousMenu = CurrentMenu;

                    PlaySound(ButtonClickSFX);
                    Transitioning = true;
                    ScreenFade = 1.0f;
                    GoingBack = true;
                }
            } else HoverTransition[(5 + 940) + 1] = CustomLerp(HoverTransition[(5 + 940) + 1], 0.0f, 0.1f);
            
            SortingSettingButtonColor = (HoverTransition[(5 + 940) + 1] > 0.5f) ? PURPLE : DARKPURPLE;
            SortingSettingTextColor = (HoverTransition[(5 + 940) + 1] > 0.5f) ? BLACK : WHITE;
                
            float scale = 1.0f + HoverTransition[(5 + 940) + 1] * 0.1f;
            float newWidth = SortingSettingButton.width * scale;
            float newHeight = SortingSettingButton.height * scale;
            float newX = SortingSettingButton.x - (newWidth - SortingSettingButton.width) / 2;
            float newY = SortingSettingButton.y - (newHeight - SortingSettingButton.height) / 2;
            
            DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, SortingSettingButtonColor);

            Vector2 textWidth = MeasureTextEx(GIFont, SortingSettingButtonText, (FONT_H3 * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_H3 * scale / 2);
            DrawTextEx(GIFont, SortingSettingButtonText, (Vector2){textX, textY}, (FONT_H3 * scale), SPACING_WIDER, SortingSettingTextColor);
        }

        {
            if (CheckCollisionPointRec(Mouse, SHOWDETAILS_GOTOPAGE_Buttons[5])) {
                HoverTransition[5 + 930] = CustomLerp(HoverTransition[5 + 930], 1.0f, 0.1f);

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {                
                    int RequestedPage = atoi(GoToPageInputBox[3].Text);
                    if (RequestedPage < 1)  { RequestedPage = 1;  }
                    if (RequestedPage > 26) { RequestedPage = 26; }

                    int MaxAvailablePage = (ReadVendors + DISPLAY_PER_PAGE - 1) / DISPLAY_PER_PAGE; // Ensures proper rounding
                    if (RequestedPage > MaxAvailablePage) { RequestedPage = MaxAvailablePage; }

                    snprintf(GoToPageInputBox[3].Text, sizeof(GoToPageInputBox[3].Text), "%02d", RequestedPage);
                    CurrentPages[3] = RequestedPage - 1;

                    PlaySound(ButtonClickSFX);
                    Transitioning = true;
                    ScreenFade = 1.0f;
                    GoingBack = true;
                }
            } else HoverTransition[5 + 930] = CustomLerp(HoverTransition[5 + 930], 0.0f, 0.1f);
            
            SHOWDETAILS_GOTOPAGE_ButtonColor[5] = (HoverTransition[5 + 930] > 0.5f) ? SKYBLUE : DARKBLUE;
            SHOWDETAILS_GOTOPAGE_TextColor[5] = (HoverTransition[5 + 930] > 0.5f) ? BLACK : WHITE;
                
            float scale = 1.0f + HoverTransition[5 + 930] * 0.1f;
            float newWidth = SHOWDETAILS_GOTOPAGE_Buttons[5].width * scale;
            float newHeight = SHOWDETAILS_GOTOPAGE_Buttons[5].height * scale;
            float newX = SHOWDETAILS_GOTOPAGE_Buttons[5].x - (newWidth - SHOWDETAILS_GOTOPAGE_Buttons[5].width) / 2;
            float newY = SHOWDETAILS_GOTOPAGE_Buttons[5].y - (newHeight - SHOWDETAILS_GOTOPAGE_Buttons[5].height) / 2;
            
            DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, SHOWDETAILS_GOTOPAGE_ButtonColor[5]);

            Vector2 textWidth = MeasureTextEx(GIFont, SHOWDETAILS_GOTOPAGE_ButtonTexts[1], (FONT_H3 * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_H3 * scale / 2);
            DrawTextEx(GIFont, SHOWDETAILS_GOTOPAGE_ButtonTexts[1], (Vector2){textX, textY}, (FONT_H3 * scale), SPACING_WIDER, SHOWDETAILS_GOTOPAGE_TextColor[5]);

            DrawRectangleRec(GoToPageInputBox[3].Box, GRAY);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (CheckCollisionPointRec(Mouse, GoToPageInputBox[3].Box)) { GoToPageInputBox[3].IsActive = true; }
                else { GoToPageInputBox[3].IsActive = false; }
            }

            if (GoToPageInputBox[3].IsActive) {
                int Key = GetCharPressed();
                while (Key >= '0' && Key <= '9') {
                    if (strlen(GoToPageInputBox[3].Text) < 2) {
                        int len = strlen(GoToPageInputBox[3].Text);
                        GoToPageInputBox[3].Text[len] = (char)Key;
                        GoToPageInputBox[3].Text[len + 1] = '\0';
                    } Key = GetCharPressed();
                }

                if (IsKeyPressed(KEY_BACKSPACE) && strlen(GoToPageInputBox[3].Text) > 0) {
                    GoToPageInputBox[3].Text[strlen(GoToPageInputBox[3].Text) - 1] = '\0';
                }
            }

            FrameCounter++; // Increment frame counter for cursor blinking
            Color boxColor = GRAY;    
            if (GoToPageInputBox[3].IsActive) boxColor = SKYBLUE; // Active input
            else if (CheckCollisionPointRec(Mouse, GoToPageInputBox[3].Box)) boxColor = YELLOW; // Hover effect

            DrawRectangleRec(GoToPageInputBox[3].Box, boxColor);
            DrawTextEx(GIFont, GoToPageInputBox[3].Text, (Vector2){GoToPageInputBox[3].Box.x + 5, GoToPageInputBox[3].Box.y + 5}, FONT_H1, SPACING_WIDER, BLACK);

            if (GoToPageInputBox[3].IsActive && (FrameCounter / CURSOR_BLINK_SPEED) % 2 == 0) {
                int textWidth = MeasureTextEx(GIFont, GoToPageInputBox[3].Text, FONT_H1, SPACING_WIDER).x;
                DrawTextEx(GIFont, "|", (Vector2){GoToPageInputBox[3].Box.x + 5 + textWidth, GoToPageInputBox[3].Box.y + 5}, FONT_H1, SPACING_WIDER, BLACK);
            } if (strlen(GoToPageInputBox[3].Text) == 0 && !GoToPageInputBox[3].IsActive) {
                DrawTextEx(GIFont, "01", (Vector2){GoToPageInputBox[3].Box.x + 5, GoToPageInputBox[3].Box.y + 5}, FONT_H1, SPACING_WIDER, Fade(BLACK, 0.6));
            }
        }

        for (int i = 0; i < UsersOnPage; i++) {
            if (CheckCollisionPointRec(Mouse, SHOWDETAILS_GOTOPAGE_Buttons[i])) {
                HoverTransition[i + 930] = CustomLerp(HoverTransition[i + 930], 1.0f, 0.1f);

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    GetTemporaryVendorIndex = i + (CurrentPages[3] * DISPLAY_PER_PAGE);
                    
                    // Get the real Vendor's ID based on the original vendors' database, not from the modifiable one.
                    for (int i = 0; i < ReadVendors; i++) {
                        if (strcmp(Vendors[GetTemporaryVendorIndex].Username, OriginalVendors[i].Username) == 0) {
                            GetTemporaryVendorIndex = i;
                            break;
                        }
                    }

                    NextMenu = MENU_MAIN_GOVERNMENT_GetVendorList_DETAILS;

                    PlaySound(ButtonClickSFX);
                    Transitioning = true;
                    ScreenFade = 1.0f;
                    GoingBack = true;
                }
            } else {
                HoverTransition[i + 930] = CustomLerp(HoverTransition[i + 930], 0.0f, 0.1f);
            }

            SHOWDETAILS_GOTOPAGE_ButtonColor[i] = (HoverTransition[i + 930] > 0.5f) ? GREEN : DARKGREEN;
            SHOWDETAILS_GOTOPAGE_TextColor[i] = (HoverTransition[i + 930] > 0.5f) ? BLACK : WHITE;

            float scale = 1.0f + HoverTransition[i + 930] * 0.1f;
            float newWidth = SHOWDETAILS_GOTOPAGE_Buttons[i].width * scale;
            float newHeight = SHOWDETAILS_GOTOPAGE_Buttons[i].height * scale;
            float newX = SHOWDETAILS_GOTOPAGE_Buttons[i].x - (newWidth - SHOWDETAILS_GOTOPAGE_Buttons[i].width) / 2;
            float newY = SHOWDETAILS_GOTOPAGE_Buttons[i].y - (newHeight - SHOWDETAILS_GOTOPAGE_Buttons[i].height) / 2;

            DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, SHOWDETAILS_GOTOPAGE_ButtonColor[i]);

            Vector2 textSize = MeasureTextEx(GIFont, SHOWDETAILS_GOTOPAGE_ButtonTexts[0], (FONT_H5 * scale), SPACING_WIDER);
            Vector2 textPosition = { newX + (newWidth - textSize.x) / 2, newY + (newHeight - FONT_H5 * scale) / 2 };

            DrawTextEx(GIFont, SHOWDETAILS_GOTOPAGE_ButtonTexts[0], textPosition, (FONT_H5 * scale), SPACING_WIDER, SHOWDETAILS_GOTOPAGE_TextColor[i]);
        }

        DrawManageViewBorder((UsersOnPage < 5) ? UsersOnPage : 4);
        for (int i = 0; i < UsersOnPage; i++) {
            snprintf(SAD_No, sizeof(SAD_No), "%03d", (i + 1) + (CurrentPages[3] * DISPLAY_PER_PAGE));
            SAD_Numberings[i].TextFill = SAD_No;
            SAD_Numberings[i].TextPosition = (Vector2){60 + LeftMargin, 240 + (144 * i) + ((TopMargin * 5) / 2) + 3};
            SAD_Numberings[i].FontData = (Vector2){FONT_H2, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_Numberings[i].TextFill, SAD_Numberings[i].TextPosition, SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y, SKYBLUE);

            SAD_FirstLine[i].TextFill = "Nama/Kode Vendor (atau Catering):";
            SAD_FirstLine[i].TextPosition = (Vector2){
                (60 + LeftMargin) + (MeasureTextEx(GIFont, "000", SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y).x + LeftMargin),
                240 + (144 * i) + (TopMargin / 2)
            };
            SAD_FirstLine[i].FontData = (Vector2){FONT_H5, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_FirstLine[i].TextFill, SAD_FirstLine[i].TextPosition, SAD_FirstLine[i].FontData.x, SAD_FirstLine[i].FontData.y, WHITE);

            SAD_VendorName[i].TextFill = Vendors[i].Username;
            SAD_VendorName[i].TextPosition = (Vector2){
                SAD_FirstLine[i].TextPosition.x,
                SAD_FirstLine[i].TextPosition.y + MeasureTextEx(GIFont, SAD_Numberings[i].TextFill, SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y).y - (TopMargin / 3)
            };
            SAD_VendorName[i].FontData = (Vector2){(strlen(SAD_VendorName[i].TextFill) <= 20) ? FONT_H1 : FONT_H2, SPACING_WIDER};
            SAD_VendorName[i].TextPosition.y += (SAD_VendorName[i].FontData.x == FONT_H1) ? 0 : 7;
            DrawTextEx(GIFont, SAD_VendorName[i].TextFill, SAD_VendorName[i].TextPosition, SAD_VendorName[i].FontData.x, SAD_VendorName[i].FontData.y, PURPLE);

            for (int c = 0; c < MAX_INPUT_LENGTH; c++) { SAD_GeneratedSecondLineMeasurement[c] = 'O'; }
            SAD_SecondLine[i].TextFill = "Ter-afiliasi dengan Sekolah:";
            SAD_SecondLine[i].TextPosition = (Vector2){
                (((60 + LeftMargin) + SAD_FirstLine[i].TextPosition.x) * 1.5) + MeasureTextEx(GIFont, SAD_GeneratedSecondLineMeasurement, SAD_FirstLine[i].FontData.x, SAD_FirstLine[i].FontData.y).x,
                240 + (144 * i) + (TopMargin / 2)
            };
            SAD_SecondLine[i].FontData = (Vector2){FONT_H5, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_SecondLine[i].TextFill, SAD_SecondLine[i].TextPosition, SAD_SecondLine[i].FontData.x, SAD_SecondLine[i].FontData.y, WHITE);
            SAD_GeneratedSecondLineMeasurement[0] = '\0';

            if (strlen(Vendors[i].AffiliatedSchoolData.Name) == 0) SAD_SchoolName[i].TextFill = "-";
            else SAD_SchoolName[i].TextFill = Vendors[i].AffiliatedSchoolData.Name;
            SAD_SchoolName[i].TextPosition = (Vector2){
                SAD_SecondLine[i].TextPosition.x,
                SAD_SecondLine[i].TextPosition.y + MeasureTextEx(GIFont, SAD_SecondLine[i].TextFill, SAD_SecondLine[i].FontData.x, SAD_SecondLine[i].FontData.y).y + (TopMargin + 5)
            };
            SAD_SchoolName[i].FontData = (Vector2){(strlen(SAD_SchoolName[i].TextFill) <= 20) ? FONT_H1 : FONT_H2, SPACING_WIDER};
            SAD_SchoolName[i].TextPosition.y -= (SAD_SchoolName[i].FontData.x == FONT_H1) ? 12 : 6;
            DrawTextEx(GIFont, SAD_SchoolName[i].TextFill, SAD_SchoolName[i].TextPosition, SAD_SchoolName[i].FontData.x, SAD_SchoolName[i].FontData.y, YELLOW);

            SAD_ThirdLine[i].TextFill = "Banyak Murid:";
            SAD_ThirdLine[i].TextPosition = (Vector2){
                SAD_SecondLine[i].TextPosition.x,
                (278 + (144 * i) + TopMargin) + MeasureTextEx(GIFont, SAD_SchoolName[i].TextFill, SAD_SchoolName[i].FontData.x, SAD_SchoolName[i].FontData.y).y + (SAD_SchoolName[i].FontData.x == FONT_H1 ? 0 : 12)
            };
            SAD_ThirdLine[i].FontData = (Vector2){FONT_H5, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_ThirdLine[i].TextFill, SAD_ThirdLine[i].TextPosition, SAD_ThirdLine[i].FontData.x, SAD_ThirdLine[i].FontData.y, WHITE);

            if (strlen(Vendors[i].AffiliatedSchoolData.Students) == 0) SAD_ManyStudents[i].TextFill = "...";
            else SAD_ManyStudents[i].TextFill = Vendors[i].AffiliatedSchoolData.Students;
            SAD_ManyStudents[i].TextPosition = (Vector2){
                SAD_ThirdLine[i].TextPosition.x + MeasureTextEx(GIFont, SAD_ThirdLine[i].TextFill, SAD_ThirdLine[i].FontData.x, SAD_ThirdLine[i].FontData.y).x + 10,
                SAD_ThirdLine[i].TextPosition.y - 2.38
            };
            SAD_ManyStudents[i].FontData = (Vector2){FONT_H4, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_ManyStudents[i].TextFill, SAD_ManyStudents[i].TextPosition, SAD_ManyStudents[i].FontData.x, SAD_ManyStudents[i].FontData.y, BEIGE);
        }

        if (IsKeyPressed(KEY_RIGHT) && UsersOnPage == DISPLAY_PER_PAGE && (CurrentPages[3] * DISPLAY_PER_PAGE) + UsersOnPage < ReadUserAccount(Vendors)) { CurrentPages[3]++; }
        if (IsKeyPressed(KEY_LEFT) && CurrentPages[3] > 0) { CurrentPages[3]--; }
        
        DrawTextEx(GIFont, ShowRegisteredVendors.TextFill, ShowRegisteredVendors.TextPosition, ShowRegisteredVendors.FontData.x, ShowRegisteredVendors.FontData.y, PINK);
        DrawTextEx(GIFont, GoToPageGuide.TextFill, GoToPageGuide.TextPosition, GoToPageGuide.FontData.x, GoToPageGuide.FontData.y, WHITE);
        DrawTextEx(GIFont, PaginationInfo.TextFill, PaginationInfo.TextPosition, PaginationInfo.FontData.x, PaginationInfo.FontData.y, WHITE);
        DrawTextEx(GIFont, PaginationGuide.TextFill, PaginationGuide.TextPosition, PaginationGuide.FontData.x, PaginationGuide.FontData.y, GREEN);
    }
}

void GovernmentViewVendorsMenu_DETAILS(void) {
    char ReturnMenuState[256] = { 0 };

    PreviousMenu = MENU_MAIN_GOVERNMENT_GetVendorList;
    DrawPreviousMenuButton();

    GetMenuState(CurrentMenu, ReturnMenuState, false);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H2, SPACING_WIDER, (Vector2){0, 60}, 0, true);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

    Vector2 Mouse = GetMousePosition();

    // Resetting the error and success messages first...
    HasClicked[7] = false; HasClicked[8] = false;
    MenusManagementInputBox_ADD[0].Text[0] = '\0'; MenusManagementInputBox_ADD[1].Text[0] = '\0'; MenusManagementInputBox_ADD[2].Text[0] = '\0'; MenusManagementInputBox_ADD[3].Text[0] = '\0'; MenusManagementInputBox_ADD[4].Text[0] = '\0';
    MenusManagementInputBox_ADD_AllValid = true;
    MenusManagementInputBox_EDIT_AllValid = true;
    // Resetting the error and success messages first...

    if (!IsCardTransitioning) {
        // if (CheckCollisionPointRec(mouse, nextButton) && currentProfile < MAX_PROFILES - 1) {
        if (IsKeyPressed(KEY_RIGHT) && CurrentCardMenuProfile < (SCHOOL_DAYS - 1)) {
            TransitionDirection = 1; // Move right
            IsCardTransitioning = true;
            TargetX = SCREEN_WIDTH; // Move to the right
        }

        // if (CheckCollisionPointRec(mouse, prevButton) && currentProfile > 0) {
        if (IsKeyPressed(KEY_LEFT) && CurrentCardMenuProfile > 0) {
            TransitionDirection = -1; // Move left
            IsCardTransitioning = true;
            TargetX = -SCREEN_WIDTH; // Move to the left
        }
    }

    Rectangle NavigationButtons[2] = {
        {75, (SCREEN_HEIGHT / 2) - (210 / 2), 90, 210},
        {(SCREEN_WIDTH - 75) - 90, (SCREEN_HEIGHT / 2) - (210 / 2), 90, 210},
    };
    Color NavigationButtonColor[2], NavigationTextColor[2];
    const char *NavigationButtonsTexts[] = {"[<]", "[>]"};

    for (int i = 801; i < 803; i++) {
        if (CheckCollisionPointRec(Mouse, NavigationButtons[i - 801])) {
            HoverTransition[i] = CustomLerp(HoverTransition[i], 1.0f, 0.1f);
            
            if (!IsCardTransitioning) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    PlaySound(ButtonClickSFX);

                    if (i == 802 && CurrentCardMenuProfile < (SCHOOL_DAYS - 1)) {
                        TransitionDirection = 1; // Move right
                        IsCardTransitioning = true;
                        TargetX = SCREEN_WIDTH; // Move to the right
                    }

                    if (i == 801 && CurrentCardMenuProfile > 0) {
                        TransitionDirection = -1; // Move left
                        IsCardTransitioning = true;
                        TargetX = -SCREEN_WIDTH; // Move to the left
                    }
                }

            }

        } else HoverTransition[i] = CustomLerp(HoverTransition[i], 0.0f, 0.1f);
        
        if (i == 801) {
            NavigationButtonColor[i - 801] = (HoverTransition[i] > 0.5f) ? PURPLE : GRAY;
            NavigationTextColor[i - 801] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        } else if (i == 802) {
            NavigationButtonColor[i - 801] = (HoverTransition[i] > 0.5f) ? SKYBLUE : GRAY;
            NavigationTextColor[i - 801] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        }

        float scale = 1.0f + HoverTransition[i] * 0.1f;
        
        float newWidth = NavigationButtons[i - 801].width * scale;
        float newHeight = NavigationButtons[i - 801].height * scale;
        float newX = NavigationButtons[i - 801].x - (newWidth - NavigationButtons[i - 801].width) / 2;
        float newY = NavigationButtons[i - 801].y - (newHeight - NavigationButtons[i - 801].height) / 2;
        
        DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, NavigationButtonColor[i - 801]);
        
        Vector2 textWidth = MeasureTextEx(GIFont, NavigationButtonsTexts[(i - 801)], (FONT_H1 * scale), SPACING_WIDER);
        int textX = newX + (newWidth / 2) - (textWidth.x / 2);
        int textY = newY + (newHeight / 2) - (FONT_H1 * scale / 2);
        DrawTextEx(GIFont, NavigationButtonsTexts[(i - 801)], (Vector2){textX, textY}, (FONT_H1 * scale), SPACING_WIDER, NavigationTextColor[i - 801]);
    }

    // LERP Animation for Smooth Transition
    if (IsCardTransitioning) {
        SetCardPositionInX = CustomLerp(SetCardPositionInX, TargetX, 0.1f);

        // Stop transition when close enough
        if (fabs(SetCardPositionInX - TargetX) < 1.0f) {
            IsCardTransitioning = false;
            CurrentCardMenuProfile += TransitionDirection;
            SetCardPositionInX = 0; // Reset position
            TargetX = 0;
        }
    }

    // Draw IsCardTransitioning Profiles
    DrawMenuProfileCard(GetTemporaryVendorIndex, CurrentCardMenuProfile, SetCardPositionInX, false, false, true);
    if (IsCardTransitioning) {
        DrawMenuProfileCard(GetTemporaryVendorIndex, CurrentCardMenuProfile + TransitionDirection, SetCardPositionInX + (-TransitionDirection * SCREEN_WIDTH), false, false, true);
    }
}

void GovernmentConfirmBudgetPlansMenu(void) {
    char ReturnMenuState[256] = { 0 };
    int LeftMargin = 40, TopMargin = 20;
    Vector2 EditDeleteButtonSize = {180, 50};
    
    Rectangle AFFILIATE_GOTOPAGE_Buttons[6] = {
        {SCREEN_WIDTH - 240 - 15, 240 + 48, EditDeleteButtonSize.x, EditDeleteButtonSize.y}, 
        {AFFILIATE_GOTOPAGE_Buttons[0].x, AFFILIATE_GOTOPAGE_Buttons[0].y + 144, EditDeleteButtonSize.x, EditDeleteButtonSize.y}, 
        {AFFILIATE_GOTOPAGE_Buttons[1].x, AFFILIATE_GOTOPAGE_Buttons[1].y + 144, EditDeleteButtonSize.x, EditDeleteButtonSize.y}, 
        {AFFILIATE_GOTOPAGE_Buttons[2].x, AFFILIATE_GOTOPAGE_Buttons[2].y + 144, EditDeleteButtonSize.x, EditDeleteButtonSize.y}, 
        {AFFILIATE_GOTOPAGE_Buttons[3].x, AFFILIATE_GOTOPAGE_Buttons[3].y + 144, EditDeleteButtonSize.x, EditDeleteButtonSize.y}, 
        {SCREEN_WIDTH - 180, 160, 120, 60}
    };
    Color AFFILIATE_GOTOPAGE_ButtonColor[6], AFFILIATE_GOTOPAGE_TextColor[6];
    const char *AFFILIATE_GOTOPAGE_ButtonTexts[] = {"[!] Periksa...", "Tuju!"};

    const char *SortingSettingButtonText = "[@] Urutkan...";
    Rectangle SortingSettingButton = {60, AFFILIATE_GOTOPAGE_Buttons[5].y, (AFFILIATE_GOTOPAGE_Buttons[5].width * 2), AFFILIATE_GOTOPAGE_Buttons[5].height};
    Color SortingSettingButtonColor, SortingSettingTextColor;

    PreviousMenu = MENU_MAIN_GOVERNMENT;
    DrawPreviousMenuButton();

    GetMenuState(CurrentMenu, ReturnMenuState, false);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H2, SPACING_WIDER, (Vector2){0, 60}, 0, true);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

    const char *DataTexts[] = {
        "Mohon maaf, saat ini belum ada data pengajuan RAB Menu M.S.G. dari pihak Vendor.",
        "Mohon menunggu untuk pemberitahuan selanjutnya dari masing-masing Vendor, atau Anda dapat memantau menu ini kerap kali.",
        "Terima kasih atas kerja samanya, Pemerintah setempat..."
    };
    TextObject NoBPRequestsMessage[3] = {
        CreateTextObject(GIFont, DataTexts[0], FONT_H5, SPACING_WIDER, (Vector2){0, (SCREEN_HEIGHT / 2) - 30}, 0, true),
        CreateTextObject(GIFont, DataTexts[1], FONT_H5, SPACING_WIDER, (Vector2){0, NoBPRequestsMessage[0].TextPosition.y}, 40, true),
        CreateTextObject(GIFont, DataTexts[2], FONT_H3, SPACING_WIDER, (Vector2){0, NoBPRequestsMessage[1].TextPosition.y}, 80, true),
    };

    int SAD_DisplayRequestedBPAmount = 0;
    char SAD_No[16] = { 0 }, SAD_GeneratedSecondLineMeasurement[32] = { 0 }, SAD_DisplayRequestedBP[64] = { 0 };
    TextObject SAD_Numberings[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_FirstLine[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_SchoolName[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_SecondLine[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_VendorName[MAX_ACCOUNT_LIMIT] = { 0 };
    
    GoToPageInputBox[5].Box = (Rectangle){AFFILIATE_GOTOPAGE_Buttons[5].x - AFFILIATE_GOTOPAGE_Buttons[5].width + 20, AFFILIATE_GOTOPAGE_Buttons[5].y, 80, 60};

    char ShowRequestedBPVendorsText[128] = { 0 }, PrepareConfirmRequestMessage[256] = { 0 };
    const char *PrepareGoToPageGuide = "Pergi ke halaman:";
    const char *PreparePaginationGuide = "Tekan [<] atau [>] di keyboard laptop/PC Anda untuk bernavigasi...";
    char PreparePaginationTextInfo[64] = { 0 };

    snprintf(ShowRequestedBPVendorsText, sizeof(ShowRequestedBPVendorsText), "Sebanyak `%d dari %d` akun Vendor (atau Catering) telah terdata...", ReadVendors, MAX_ACCOUNT_LIMIT);
    TextObject ShowRequestedBPVendors = CreateTextObject(GIFont, ShowRequestedBPVendorsText, FONT_P, SPACING_WIDER, (Vector2){0, 200}, 0, true);
    snprintf(PrepareConfirmRequestMessage, sizeof(PrepareConfirmRequestMessage), "Terdapat [%d] data pengajuan Menu M.S.G. dari para Vendor yang belum DIPROSES.\nApakah Anda ingin memproses masing-masing pengajuan lebih lanjut?", ReadRequestedBPFromVendors);
    TextObject PrepareConfirmRequest = CreateTextObject(GIFont, PrepareConfirmRequestMessage, FONT_H5, SPACING_WIDER, (Vector2){60, 980}, 0, false);
    snprintf(PreparePaginationTextInfo, sizeof(PreparePaginationTextInfo), "Halaman %d dari %d", (CurrentPages[5] + 1), (ReadRequestedBPFromVendors % DISPLAY_PER_PAGE != 0) ? ((ReadRequestedBPFromVendors / DISPLAY_PER_PAGE) + 1) : (ReadRequestedBPFromVendors / DISPLAY_PER_PAGE));
    TextObject GoToPageGuide = CreateTextObject(GIFont, PrepareGoToPageGuide, FONT_H5, SPACING_WIDER, (Vector2){GoToPageInputBox[5].Box.x, GoToPageInputBox[5].Box.y - (GoToPageInputBox[5].Box.height / 2)}, 0, false);
    TextObject PaginationInfo = CreateTextObject(GIFont, PreparePaginationTextInfo, FONT_P, SPACING_WIDER, (Vector2){SCREEN_WIDTH - 60 - MeasureTextEx(GIFont, PreparePaginationTextInfo, FONT_P, SPACING_WIDER).x, 980}, 0, false);
    TextObject PaginationGuide = CreateTextObject(GIFont, PreparePaginationGuide, FONT_H5, SPACING_WIDER, (Vector2){SCREEN_WIDTH - 60 - MeasureTextEx(GIFont, PreparePaginationGuide, FONT_H5, SPACING_WIDER).x, PaginationInfo.TextPosition.y}, 40, false);
    
    ReadRequestedBPFromVendors = ReadRequestingBPVendors(RequestedBPFromVendors);
    RequestedBPFromVendorsOnPage = ReadRequestingBPVendorsWithPagination(CurrentPages[5], RequestedBPFromVendors);

    // Always reset the menu profile cards...
    // GetTemporaryVendorIndex = -1;
    IsCardTransitioning = false;
    CurrentCardMenuProfile = 0;
    SetCardPositionInX = 0; // Start off-screen left
    TargetX = 0;
    TransitionDirection = 0; // 1 = right, -1 = left
    // Always reset the menu profile cards...
    
    Vector2 Mouse = GetMousePosition();
    if (ReadRequestedBPFromVendors == 0) {
        // DrawManageViewBorder(0);
        for (int i = 0; i < 3; i++) {
            DrawTextEx(GIFont, NoBPRequestsMessage[i].TextFill, NoBPRequestsMessage[i].TextPosition, NoBPRequestsMessage[i].FontData.x, NoBPRequestsMessage[i].FontData.y, (i != 2) ? YELLOW : WHITE);
        }
    } else {
        {
            if (CheckCollisionPointRec(Mouse, SortingSettingButton)) {
                HoverTransition[(5 + 940) + 1] = CustomLerp(HoverTransition[(5 + 940) + 1], 1.0f, 0.1f);

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {                
                    NextMenu = MENU_SUBMAIN_SortingOptions;
                    PreviousMenu = CurrentMenu;

                    PlaySound(ButtonClickSFX);
                    Transitioning = true;
                    ScreenFade = 1.0f;
                    GoingBack = true;
                }
            } else HoverTransition[(5 + 940) + 1] = CustomLerp(HoverTransition[(5 + 940) + 1], 0.0f, 0.1f);
            
            SortingSettingButtonColor = (HoverTransition[(5 + 940) + 1] > 0.5f) ? PURPLE : DARKPURPLE;
            SortingSettingTextColor = (HoverTransition[(5 + 940) + 1] > 0.5f) ? BLACK : WHITE;
                
            float scale = 1.0f + HoverTransition[(5 + 940) + 1] * 0.1f;
            float newWidth = SortingSettingButton.width * scale;
            float newHeight = SortingSettingButton.height * scale;
            float newX = SortingSettingButton.x - (newWidth - SortingSettingButton.width) / 2;
            float newY = SortingSettingButton.y - (newHeight - SortingSettingButton.height) / 2;
            
            DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, SortingSettingButtonColor);

            Vector2 textWidth = MeasureTextEx(GIFont, SortingSettingButtonText, (FONT_H3 * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_H3 * scale / 2);
            DrawTextEx(GIFont, SortingSettingButtonText, (Vector2){textX, textY}, (FONT_H3 * scale), SPACING_WIDER, SortingSettingTextColor);
        }

        {
            DrawRectangleRec(GoToPageInputBox[5].Box, GRAY);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (CheckCollisionPointRec(Mouse, GoToPageInputBox[5].Box)) { GoToPageInputBox[5].IsActive = true; }
                else { GoToPageInputBox[5].IsActive = false; }
            }

            if (GoToPageInputBox[5].IsActive) {
                int Key = GetCharPressed();
                while (Key >= '0' && Key <= '9') {
                    if (strlen(GoToPageInputBox[5].Text) < 2) {
                        int len = strlen(GoToPageInputBox[5].Text);
                        GoToPageInputBox[5].Text[len] = (char)Key;
                        GoToPageInputBox[5].Text[len + 1] = '\0';
                    } Key = GetCharPressed();
                }

                if (IsKeyPressed(KEY_BACKSPACE) && strlen(GoToPageInputBox[5].Text) > 0) {
                    GoToPageInputBox[5].Text[strlen(GoToPageInputBox[5].Text) - 1] = '\0';
                }
            }

            FrameCounter++; // Increment frame counter for cursor blinking
            Color boxColor = GRAY;    
            if (GoToPageInputBox[5].IsActive) boxColor = SKYBLUE; // Active input
            else if (CheckCollisionPointRec(Mouse, GoToPageInputBox[5].Box)) boxColor = YELLOW; // Hover effect

            DrawRectangleRec(GoToPageInputBox[5].Box, boxColor);
            DrawTextEx(GIFont, GoToPageInputBox[5].Text, (Vector2){GoToPageInputBox[5].Box.x + 5, GoToPageInputBox[5].Box.y + 5}, FONT_H1, SPACING_WIDER, BLACK);

            if (GoToPageInputBox[5].IsActive && (FrameCounter / CURSOR_BLINK_SPEED) % 2 == 0) {
                int textWidth = MeasureTextEx(GIFont, GoToPageInputBox[5].Text, FONT_H1, SPACING_WIDER).x;
                DrawTextEx(GIFont, "|", (Vector2){GoToPageInputBox[5].Box.x + 5 + textWidth, GoToPageInputBox[5].Box.y + 5}, FONT_H1, SPACING_WIDER, BLACK);
            } if (strlen(GoToPageInputBox[5].Text) == 0 && !GoToPageInputBox[5].IsActive) {
                DrawTextEx(GIFont, "01", (Vector2){GoToPageInputBox[5].Box.x + 5, GoToPageInputBox[5].Box.y + 5}, FONT_H1, SPACING_WIDER, Fade(BLACK, 0.6));
            }
        }

        {
            if (CheckCollisionPointRec(Mouse, AFFILIATE_GOTOPAGE_Buttons[5])) {
                HoverTransition[5 + 940] = CustomLerp(HoverTransition[5 + 940], 1.0f, 0.1f);

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {                
                    int RequestedPage = atoi(GoToPageInputBox[5].Text);
                    if (RequestedPage < 1)  { RequestedPage = 1;  }
                    if (RequestedPage > 26) { RequestedPage = 26; }

                    int MaxAvailablePage = (ReadRequestedBPFromVendors + DISPLAY_PER_PAGE - 1) / DISPLAY_PER_PAGE; // Ensures proper rounding
                    if (RequestedPage > MaxAvailablePage) { RequestedPage = MaxAvailablePage; }

                    snprintf(GoToPageInputBox[5].Text, sizeof(GoToPageInputBox[5].Text), "%02d", RequestedPage);
                    CurrentPages[5] = RequestedPage - 1;

                    PlaySound(ButtonClickSFX);
                    Transitioning = true;
                    ScreenFade = 1.0f;
                    GoingBack = true;
                }
            } else HoverTransition[5 + 940] = CustomLerp(HoverTransition[5 + 940], 0.0f, 0.1f);
            
            AFFILIATE_GOTOPAGE_ButtonColor[5] = (HoverTransition[5 + 940] > 0.5f) ? SKYBLUE : DARKBLUE;
            AFFILIATE_GOTOPAGE_TextColor[5] = (HoverTransition[5 + 940] > 0.5f) ? BLACK : WHITE;
                
            float scale = 1.0f + HoverTransition[5 + 940] * 0.1f;
            float newWidth = AFFILIATE_GOTOPAGE_Buttons[5].width * scale;
            float newHeight = AFFILIATE_GOTOPAGE_Buttons[5].height * scale;
            float newX = AFFILIATE_GOTOPAGE_Buttons[5].x - (newWidth - AFFILIATE_GOTOPAGE_Buttons[5].width) / 2;
            float newY = AFFILIATE_GOTOPAGE_Buttons[5].y - (newHeight - AFFILIATE_GOTOPAGE_Buttons[5].height) / 2;
            
            DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, AFFILIATE_GOTOPAGE_ButtonColor[5]);

            Vector2 textWidth = MeasureTextEx(GIFont, AFFILIATE_GOTOPAGE_ButtonTexts[1], (FONT_H3 * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_H3 * scale / 2);
            DrawTextEx(GIFont, AFFILIATE_GOTOPAGE_ButtonTexts[1], (Vector2){textX, textY}, (FONT_H3 * scale), SPACING_WIDER, AFFILIATE_GOTOPAGE_TextColor[5]);
        }

        for (int i = 0; i < RequestedBPFromVendorsOnPage; i++) {
            if (CheckCollisionPointRec(Mouse, AFFILIATE_GOTOPAGE_Buttons[i])) {
                HoverTransition[i + 940] = CustomLerp(HoverTransition[i + 940], 1.0f, 0.1f);

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    GetTemporaryVendorIndex = i + (CurrentPages[5] * DISPLAY_PER_PAGE);

                    // Get the real Vendor's ID based on the original vendors' database, not from the modifiable one.
                    for (int i = 0; i < ReadRequestedBPFromVendors; i++) {
                        if (strcmp(RequestedBPFromVendors[GetTemporaryVendorIndex].Username, OriginalVendors[i].Username) == 0) {
                            GetTemporaryVendorIndex = i;
                            break;
                        }
                    }

                    NextMenu = MENU_MAIN_GOVERNMENT_ConfirmBudgetPlan_CHOOSING;

                    PlaySound(ButtonClickSFX);
                    Transitioning = true;
                    ScreenFade = 1.0f;
                    GoingBack = true;
                }
            } else {
                HoverTransition[i + 940] = CustomLerp(HoverTransition[i + 940], 0.0f, 0.1f);
            }

            AFFILIATE_GOTOPAGE_ButtonColor[i] = (HoverTransition[i + 940] > 0.5f) ? SKYBLUE : DARKBLUE;
            AFFILIATE_GOTOPAGE_TextColor[i] = (HoverTransition[i + 940] > 0.5f) ? BLACK : WHITE;

            float scale = 1.0f + HoverTransition[i + 940] * 0.1f;
            float newWidth = AFFILIATE_GOTOPAGE_Buttons[i].width * scale;
            float newHeight = AFFILIATE_GOTOPAGE_Buttons[i].height * scale;
            float newX = AFFILIATE_GOTOPAGE_Buttons[i].x - (newWidth - AFFILIATE_GOTOPAGE_Buttons[i].width) / 2;
            float newY = AFFILIATE_GOTOPAGE_Buttons[i].y - (newHeight - AFFILIATE_GOTOPAGE_Buttons[i].height) / 2;

            DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, AFFILIATE_GOTOPAGE_ButtonColor[i]);

            Vector2 textSize = MeasureTextEx(GIFont, AFFILIATE_GOTOPAGE_ButtonTexts[0], (FONT_H5 * scale), SPACING_WIDER);
            Vector2 textPosition = { newX + (newWidth - textSize.x) / 2, newY + (newHeight - FONT_H5 * scale) / 2 };

            DrawTextEx(GIFont, AFFILIATE_GOTOPAGE_ButtonTexts[0], textPosition, (FONT_H5 * scale), SPACING_WIDER, AFFILIATE_GOTOPAGE_TextColor[i]);
        }

        DrawManageViewBorder((RequestedBPFromVendorsOnPage < 5) ? RequestedBPFromVendorsOnPage : 4);
        for (int i = 0; i < RequestedBPFromVendorsOnPage; i++) {
            snprintf(SAD_No, sizeof(SAD_No), "%03d", (i + 1) + (CurrentPages[5] * DISPLAY_PER_PAGE));
            SAD_Numberings[i].TextFill = SAD_No;
            SAD_Numberings[i].TextPosition = (Vector2){60 + LeftMargin, 240 + (144 * i) + ((TopMargin * 5) / 2) + 3};
            SAD_Numberings[i].FontData = (Vector2){FONT_H2, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_Numberings[i].TextFill, SAD_Numberings[i].TextPosition, SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y, SKYBLUE);

            SAD_FirstLine[i].TextFill = "Nama/Kode Vendor (atau Catering):";
            SAD_FirstLine[i].TextPosition = (Vector2){
                (60 + LeftMargin) + (MeasureTextEx(GIFont, "000", SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y).x + LeftMargin),
                240 + (144 * i) + (TopMargin / 2)
            };
            SAD_FirstLine[i].FontData = (Vector2){FONT_H5, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_FirstLine[i].TextFill, SAD_FirstLine[i].TextPosition, SAD_FirstLine[i].FontData.x, SAD_FirstLine[i].FontData.y, WHITE);

            SAD_VendorName[i].TextFill = RequestedBPFromVendors[i].Username;
            SAD_VendorName[i].TextPosition = (Vector2){
                SAD_FirstLine[i].TextPosition.x,
                SAD_FirstLine[i].TextPosition.y + MeasureTextEx(GIFont, SAD_Numberings[i].TextFill, SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y).y - (TopMargin / 3)
            };
            SAD_VendorName[i].FontData = (Vector2){(strlen(SAD_VendorName[i].TextFill) <= 20) ? FONT_H1 : FONT_H2, SPACING_WIDER};
            SAD_VendorName[i].TextPosition.y += (SAD_VendorName[i].FontData.x == FONT_H1) ? 0 : 7;
            DrawTextEx(GIFont, SAD_VendorName[i].TextFill, SAD_VendorName[i].TextPosition, SAD_VendorName[i].FontData.x, SAD_VendorName[i].FontData.y, PURPLE);

            for (int c = 0; c < MAX_INPUT_LENGTH; c++) { SAD_GeneratedSecondLineMeasurement[c] = 'O'; }
            SAD_SecondLine[i].TextFill = "Banyak Pengajuan RAB Menu:";
            SAD_SecondLine[i].TextPosition = (Vector2){
                (((60 + LeftMargin) + SAD_FirstLine[i].TextPosition.x) * 1.5) + MeasureTextEx(GIFont, SAD_GeneratedSecondLineMeasurement, SAD_FirstLine[i].FontData.x, SAD_FirstLine[i].FontData.y).x,
                240 + (144 * i) + (TopMargin / 2)
            };
            SAD_SecondLine[i].FontData = (Vector2){FONT_H5, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_SecondLine[i].TextFill, SAD_SecondLine[i].TextPosition, SAD_SecondLine[i].FontData.x, SAD_SecondLine[i].FontData.y, WHITE);
            SAD_GeneratedSecondLineMeasurement[0] = '\0';

            if (RequestedBPFromVendors[i].BudgetPlanConfirmation[0] == 1) SAD_DisplayRequestedBPAmount++;
            if (RequestedBPFromVendors[i].BudgetPlanConfirmation[1] == 1) SAD_DisplayRequestedBPAmount++;
            if (RequestedBPFromVendors[i].BudgetPlanConfirmation[2] == 1) SAD_DisplayRequestedBPAmount++;
            if (RequestedBPFromVendors[i].BudgetPlanConfirmation[3] == 1) SAD_DisplayRequestedBPAmount++;
            if (RequestedBPFromVendors[i].BudgetPlanConfirmation[4] == 1) SAD_DisplayRequestedBPAmount++;
            snprintf(SAD_DisplayRequestedBP, sizeof(SAD_DisplayRequestedBP), "`%d dari 5` Menu Harian", SAD_DisplayRequestedBPAmount);
            SAD_SchoolName[i].TextFill = SAD_DisplayRequestedBP;
            SAD_SchoolName[i].TextPosition = (Vector2){
                SAD_SecondLine[i].TextPosition.x,
                SAD_SecondLine[i].TextPosition.y + MeasureTextEx(GIFont, SAD_SecondLine[i].TextFill, SAD_SecondLine[i].FontData.x, SAD_SecondLine[i].FontData.y).y + (TopMargin + 5) - 12
            };
            SAD_SchoolName[i].FontData = (Vector2){FONT_H1, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_SchoolName[i].TextFill, SAD_SchoolName[i].TextPosition, SAD_SchoolName[i].FontData.x, SAD_SchoolName[i].FontData.y, YELLOW);
            SAD_DisplayRequestedBPAmount = 0;
            SAD_DisplayRequestedBP[0] = '\0';

            // SAD_ThirdLine[i].TextFill = "Banyak Murid:";
            // SAD_ThirdLine[i].TextPosition = (Vector2){
            //     SAD_SecondLine[i].TextPosition.x,
            //     (278 + (144 * i) + TopMargin) + MeasureTextEx(GIFont, SAD_SchoolName[i].TextFill, SAD_SchoolName[i].FontData.x, SAD_SchoolName[i].FontData.y).y + (SAD_SchoolName[i].FontData.x == FONT_H1 ? 0 : 12)
            // };
            // SAD_ThirdLine[i].FontData = (Vector2){FONT_H5, SPACING_WIDER};
            // DrawTextEx(GIFont, SAD_ThirdLine[i].TextFill, SAD_ThirdLine[i].TextPosition, SAD_ThirdLine[i].FontData.x, SAD_ThirdLine[i].FontData.y, WHITE);

            // if (strlen(Vendors[i].AffiliatedSchoolData.Students) == 0) SAD_ManyStudents[i].TextFill = "...";
            // else SAD_ManyStudents[i].TextFill = Vendors[i].AffiliatedSchoolData.Students;
            // SAD_ManyStudents[i].TextPosition = (Vector2){
            //     SAD_ThirdLine[i].TextPosition.x + MeasureTextEx(GIFont, SAD_ThirdLine[i].TextFill, SAD_ThirdLine[i].FontData.x, SAD_ThirdLine[i].FontData.y).x + 10,
            //     SAD_ThirdLine[i].TextPosition.y - 2.38
            // };
            // SAD_ManyStudents[i].FontData = (Vector2){FONT_H4, SPACING_WIDER};
            // DrawTextEx(GIFont, SAD_ManyStudents[i].TextFill, SAD_ManyStudents[i].TextPosition, SAD_ManyStudents[i].FontData.x, SAD_ManyStudents[i].FontData.y, BEIGE);
        }

        if (IsKeyPressed(KEY_RIGHT) && RequestedBPFromVendorsOnPage == DISPLAY_PER_PAGE && (CurrentPages[5] * DISPLAY_PER_PAGE) + RequestedBPFromVendorsOnPage < ReadUserAccount(Vendors)) { CurrentPages[5]++; }
        if (IsKeyPressed(KEY_LEFT) && CurrentPages[5] > 0) { CurrentPages[5]--; }
        
        DrawTextEx(GIFont, ShowRequestedBPVendors.TextFill, ShowRequestedBPVendors.TextPosition, ShowRequestedBPVendors.FontData.x, ShowRequestedBPVendors.FontData.y, PINK);
        DrawTextEx(GIFont, PrepareConfirmRequest.TextFill, PrepareConfirmRequest.TextPosition, PrepareConfirmRequest.FontData.x, PrepareConfirmRequest.FontData.y, ORANGE);
        DrawTextEx(GIFont, GoToPageGuide.TextFill, GoToPageGuide.TextPosition, GoToPageGuide.FontData.x, GoToPageGuide.FontData.y, WHITE);
        DrawTextEx(GIFont, PaginationInfo.TextFill, PaginationInfo.TextPosition, PaginationInfo.FontData.x, PaginationInfo.FontData.y, WHITE);
        DrawTextEx(GIFont, PaginationGuide.TextFill, PaginationGuide.TextPosition, PaginationGuide.FontData.x, PaginationGuide.FontData.y, GREEN);
    }
}

void GovernmentConfirmBudgetPlansMenu_CHOOSING(void) {
    char ReturnMenuState[256] = { 0 };

    PreviousMenu = MENU_MAIN_GOVERNMENT_ConfirmBudgetPlan;
    DrawPreviousMenuButton();

    GetMenuState(CurrentMenu, ReturnMenuState, false);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H2, SPACING_WIDER, (Vector2){0, 60}, 0, true);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

    Vector2 Mouse = GetMousePosition();

    Rectangle NavigationButtons[2] = {
        {75, (SCREEN_HEIGHT / 2) - (210 / 2), 90, 210},
        {(SCREEN_WIDTH - 75) - 90, (SCREEN_HEIGHT / 2) - (210 / 2), 90, 210},
    };
    Color NavigationButtonColor[2], NavigationTextColor[2];
    const char *NavigationButtonsTexts[] = {"[<]", "[>]"};

    for (int i = 801; i < 803; i++) {
        if (CheckCollisionPointRec(Mouse, NavigationButtons[i - 801])) {
            HoverTransition[i] = CustomLerp(HoverTransition[i], 1.0f, 0.1f);
            
            if (!IsCardTransitioning) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    PlaySound(ButtonClickSFX);

                    if (i == 802 && CurrentCardMenuProfile < (SCHOOL_DAYS - 1)) {
                        TransitionDirection = 1; // Move right
                        IsCardTransitioning = true;
                        TargetX = SCREEN_WIDTH; // Move to the right
                    }

                    if (i == 801 && CurrentCardMenuProfile > 0) {
                        TransitionDirection = -1; // Move left
                        IsCardTransitioning = true;
                        TargetX = -SCREEN_WIDTH; // Move to the left
                    }
                }

            }

        } else HoverTransition[i] = CustomLerp(HoverTransition[i], 0.0f, 0.1f);
        
        if (i == 801) {
            NavigationButtonColor[i - 801] = (HoverTransition[i] > 0.5f) ? PURPLE : GRAY;
            NavigationTextColor[i - 801] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        } else if (i == 802) {
            NavigationButtonColor[i - 801] = (HoverTransition[i] > 0.5f) ? SKYBLUE : GRAY;
            NavigationTextColor[i - 801] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        }

        float scale = 1.0f + HoverTransition[i] * 0.1f;
        
        float newWidth = NavigationButtons[i - 801].width * scale;
        float newHeight = NavigationButtons[i - 801].height * scale;
        float newX = NavigationButtons[i - 801].x - (newWidth - NavigationButtons[i - 801].width) / 2;
        float newY = NavigationButtons[i - 801].y - (newHeight - NavigationButtons[i - 801].height) / 2;
        
        DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, NavigationButtonColor[i - 801]);
        
        Vector2 textWidth = MeasureTextEx(GIFont, NavigationButtonsTexts[(i - 801)], (FONT_H1 * scale), SPACING_WIDER);
        int textX = newX + (newWidth / 2) - (textWidth.x / 2);
        int textY = newY + (newHeight / 2) - (FONT_H1 * scale / 2);
        DrawTextEx(GIFont, NavigationButtonsTexts[(i - 801)], (Vector2){textX, textY}, (FONT_H1 * scale), SPACING_WIDER, NavigationTextColor[i - 801]);
    }

    if (!IsCardTransitioning) {
        // if (CheckCollisionPointRec(mouse, nextButton) && currentProfile < MAX_PROFILES - 1) {
        if (IsKeyPressed(KEY_RIGHT) && CurrentCardMenuProfile < (SCHOOL_DAYS - 1)) {
            TransitionDirection = 1; // Move right
            IsCardTransitioning = true;
            TargetX = SCREEN_WIDTH; // Move to the right
        }

        // if (CheckCollisionPointRec(mouse, prevButton) && currentProfile > 0) {
        if (IsKeyPressed(KEY_LEFT) && CurrentCardMenuProfile > 0) {
            TransitionDirection = -1; // Move left
            IsCardTransitioning = true;
            TargetX = -SCREEN_WIDTH; // Move to the left
        }
    } else {
        // LERP Animation for Smooth Transition
        SetCardPositionInX = CustomLerp(SetCardPositionInX, TargetX, 0.1f);

        // Stop transition when close enough
        if (fabs(SetCardPositionInX - TargetX) < 1.0f) {
            IsCardTransitioning = false;
            CurrentCardMenuProfile += TransitionDirection;
            SetCardPositionInX = 0; // Reset position
            TargetX = 0;
        }
    }

    // Draw IsCardTransitioning Profiles
    DrawMenuProfileCard(GetTemporaryVendorIndex, CurrentCardMenuProfile, SetCardPositionInX, false, true, false);
    if (IsCardTransitioning) {
        DrawMenuProfileCard(GetTemporaryVendorIndex, CurrentCardMenuProfile + TransitionDirection, SetCardPositionInX + (-TransitionDirection * SCREEN_WIDTH), false, true, false);
    }
}

void GovernmentViewBilateralsMenu(void) {
    char ReturnMenuState[256] = { 0 };
    int LeftMargin = 40, TopMargin = 20;
    
    Rectangle GOTOPAGE_Buttons = {SCREEN_WIDTH - 180, 160, 120, 60};
    Color GOTOPAGE_ButtonColor, GOTOPAGE_TextColor;
    const char *GOTOPAGE_ButtonTexts = "Tuju!";

    const char *SortingSettingButtonText = "[@] Urutkan...";
    Rectangle SortingSettingButton = {60, GOTOPAGE_Buttons.y, (GOTOPAGE_Buttons.width * 2), GOTOPAGE_Buttons.height};
    Color SortingSettingButtonColor, SortingSettingTextColor;

    PreviousMenu = MENU_MAIN_GOVERNMENT;
    DrawPreviousMenuButton();

    GetMenuState(CurrentMenu, ReturnMenuState, false);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H2, SPACING_WIDER, (Vector2){0, 60}, 0, true);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

    const char *DataTexts[] = {
        "Mohon maaf, saat ini sistem tidak dapat menampilkan data yang diperlukan untuk proses bilaretal D'Magis, dikarenakan:",
        "1. Keperluan data dari pihak Vendor (atau Catering) BELUM tersedia,",
        "2. Keperluan data dari pihak Sekolah BELUM tersedia, atau",
        "3. Keperluan data kerja sama BELUM tersedia.",
        "Pastikan kondisi dari kedua ketentuan di atas untuk DIPENUHI terlebih dahulu, dikarenakan proses bilateral ini membutuhkan",
        "kerja sama dari kedua belah pihak dan TANPA PAKSAAN sekalipun.",
        "Untuk menyelesaikan masalah ini, dari pihak Anda dapat melakukan pendaftaran data Sekolah baru pada menu `[MANAGE]: Data Kerja Sama`,",
        "sedangkan teruntuk data para Vendor diharuskan menunggu inisiatif dari merekanya sendiri...",
        "Terima kasih atas kerja samanya, Pemerintah setempat..."
    };
    TextObject UnfulfilledRequestsMessage[9] = {
        CreateTextObject(GIFont, DataTexts[0], FONT_H4, SPACING_WIDER, (Vector2){0, 260}, 0, true),
        CreateTextObject(GIFont, DataTexts[1], FONT_H3, SPACING_WIDER, (Vector2){0, UnfulfilledRequestsMessage[0].TextPosition.y}, 40, true),
        CreateTextObject(GIFont, DataTexts[2], FONT_H3, SPACING_WIDER, (Vector2){0, UnfulfilledRequestsMessage[1].TextPosition.y}, 40, true),
        CreateTextObject(GIFont, DataTexts[3], FONT_H3, SPACING_WIDER, (Vector2){0, UnfulfilledRequestsMessage[2].TextPosition.y}, 40, true),
        
        CreateTextObject(GIFont, DataTexts[4], FONT_H5, SPACING_WIDER, (Vector2){0, (SCREEN_HEIGHT / 2)}, 0, true),
        CreateTextObject(GIFont, DataTexts[5], FONT_H5, SPACING_WIDER, (Vector2){0, UnfulfilledRequestsMessage[4].TextPosition.y}, 30, true),
        CreateTextObject(GIFont, DataTexts[6], FONT_H5, SPACING_WIDER, (Vector2){0, UnfulfilledRequestsMessage[5].TextPosition.y}, 60, true),
        CreateTextObject(GIFont, DataTexts[7], FONT_H5, SPACING_WIDER, (Vector2){0, UnfulfilledRequestsMessage[6].TextPosition.y}, 30, true),
        
        CreateTextObject(GIFont, DataTexts[8], FONT_H3, SPACING_WIDER, (Vector2){0, SCREEN_HEIGHT - 200}, 0, true),
    };

    char SAD_No[16] = { 0 }, SAD_GeneratedSecondLineMeasurement[32] = { 0 };
    TextObject SAD_Numberings[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_FirstLine[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_SchoolName[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_SecondLine[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_VendorName[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_ThirdLine[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject SAD_ManyStudents[MAX_ACCOUNT_LIMIT] = { 0 };
    
    GoToPageInputBox[4].Box = (Rectangle){GOTOPAGE_Buttons.x - GOTOPAGE_Buttons.width + 20, GOTOPAGE_Buttons.y, 80, 60};

    char ShowRegisteredSchoolsText[128] = { 0 };
    const char *PrepareGoToPageGuide = "Pergi ke halaman:";
    const char *PreparePaginationGuide = "Tekan [<] atau [>] di keyboard laptop/PC Anda untuk bernavigasi...";
    char PrepareAffiliationMessage[256] = { 0 };
    char PreparePaginationTextInfo[64] = { 0 };

    snprintf(ShowRegisteredSchoolsText, sizeof(ShowRegisteredSchoolsText), "Sebanyak (%d/%d) data Vendor dan (%d/%d) data Sekolah telah terdata...", ReadVendors, MAX_ACCOUNT_LIMIT, ReadSchools, MAX_ACCOUNT_LIMIT);
    TextObject ShowRegisteredSchools = CreateTextObject(GIFont, ShowRegisteredSchoolsText, FONT_P, SPACING_WIDER, (Vector2){0, 200}, 0, true);
    snprintf(PrepareAffiliationMessage, sizeof(PrepareAffiliationMessage), "Terdapat [%d] data afiliasi antar Vendor dengan Sekolah yang SUDAH tercatat.", ReadOnlyAffiliatedSchools);
    TextObject AffiliationMessage = CreateTextObject(GIFont, PrepareAffiliationMessage, FONT_H5, SPACING_WIDER, (Vector2){60, 980}, 0, false);
    snprintf(PreparePaginationTextInfo, sizeof(PreparePaginationTextInfo), "Halaman %d dari %d", (CurrentPages[4] + 1), (ReadOnlyAffiliatedSchools % DISPLAY_PER_PAGE != 0) ? ((ReadOnlyAffiliatedSchools / DISPLAY_PER_PAGE) + 1) : (ReadOnlyAffiliatedSchools / DISPLAY_PER_PAGE));
    TextObject GoToPageGuide = CreateTextObject(GIFont, PrepareGoToPageGuide, FONT_H5, SPACING_WIDER, (Vector2){GoToPageInputBox[4].Box.x, GoToPageInputBox[4].Box.y - (GoToPageInputBox[4].Box.height / 2)}, 0, false);
    TextObject PaginationInfo = CreateTextObject(GIFont, PreparePaginationTextInfo, FONT_P, SPACING_WIDER, (Vector2){SCREEN_WIDTH - 60 - MeasureTextEx(GIFont, PreparePaginationTextInfo, FONT_P, SPACING_WIDER).x, 980}, 0, false);
    TextObject PaginationGuide = CreateTextObject(GIFont, PreparePaginationGuide, FONT_H5, SPACING_WIDER, (Vector2){SCREEN_WIDTH - 60 - MeasureTextEx(GIFont, PreparePaginationGuide, FONT_H5, SPACING_WIDER).x, PaginationInfo.TextPosition.y}, 40, false);

    ReadVendors = ReadUserAccount(Vendors);
    ReadSchools = ReadSchoolsData(Schools);
    ReadOnlyAffiliatedSchools = ReadAffiliatedSchools(OnlyAffiliatedSchools);
    OnlyAffiliatedSchoolsOnPage = ReadAffiliatedSchoolsWithPagination(CurrentPages[4], OnlyAffiliatedSchools);
    
    Vector2 Mouse = GetMousePosition();
    if (ReadOnlyAffiliatedSchools == 0) {
        for (int i = 0; i < 9; i++) {
            DrawTextEx(GIFont, UnfulfilledRequestsMessage[i].TextFill, UnfulfilledRequestsMessage[i].TextPosition, UnfulfilledRequestsMessage[i].FontData.x, UnfulfilledRequestsMessage[i].FontData.y, (i == 0) ? YELLOW : (i >= 1 && i <= 3) ? PINK : (i == 4 || i == 5) ? GREEN : (i == 6 || i == 7) ? SKYBLUE : WHITE);
        }
    } else {
        {
            if (CheckCollisionPointRec(Mouse, SortingSettingButton)) {
                HoverTransition[(5 + 940) + 1] = CustomLerp(HoverTransition[(5 + 940) + 1], 1.0f, 0.1f);

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {                
                    NextMenu = MENU_SUBMAIN_SortingOptions;
                    PreviousMenu = CurrentMenu;

                    PlaySound(ButtonClickSFX);
                    Transitioning = true;
                    ScreenFade = 1.0f;
                    GoingBack = true;
                }
            } else HoverTransition[(5 + 940) + 1] = CustomLerp(HoverTransition[(5 + 940) + 1], 0.0f, 0.1f);
            
            SortingSettingButtonColor = (HoverTransition[(5 + 940) + 1] > 0.5f) ? PURPLE : DARKPURPLE;
            SortingSettingTextColor = (HoverTransition[(5 + 940) + 1] > 0.5f) ? BLACK : WHITE;
                
            float scale = 1.0f + HoverTransition[(5 + 940) + 1] * 0.1f;
            float newWidth = SortingSettingButton.width * scale;
            float newHeight = SortingSettingButton.height * scale;
            float newX = SortingSettingButton.x - (newWidth - SortingSettingButton.width) / 2;
            float newY = SortingSettingButton.y - (newHeight - SortingSettingButton.height) / 2;
            
            DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, SortingSettingButtonColor);

            Vector2 textWidth = MeasureTextEx(GIFont, SortingSettingButtonText, (FONT_H3 * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_H3 * scale / 2);
            DrawTextEx(GIFont, SortingSettingButtonText, (Vector2){textX, textY}, (FONT_H3 * scale), SPACING_WIDER, SortingSettingTextColor);
        }

        {
            DrawRectangleRec(GoToPageInputBox[4].Box, GRAY);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (CheckCollisionPointRec(Mouse, GoToPageInputBox[4].Box)) { GoToPageInputBox[4].IsActive = true; }
                else { GoToPageInputBox[4].IsActive = false; }
            }

            if (GoToPageInputBox[4].IsActive) {
                int Key = GetCharPressed();
                while (Key >= '0' && Key <= '9') {
                    if (strlen(GoToPageInputBox[4].Text) < 2) {
                        int len = strlen(GoToPageInputBox[4].Text);
                        GoToPageInputBox[4].Text[len] = (char)Key;
                        GoToPageInputBox[4].Text[len + 1] = '\0';
                    } Key = GetCharPressed();
                }

                if (IsKeyPressed(KEY_BACKSPACE) && strlen(GoToPageInputBox[4].Text) > 0) {
                    GoToPageInputBox[4].Text[strlen(GoToPageInputBox[4].Text) - 1] = '\0';
                }
            }

            FrameCounter++; // Increment frame counter for cursor blinking
            Color boxColor = GRAY;    
            if (GoToPageInputBox[4].IsActive) boxColor = SKYBLUE; // Active input
            else if (CheckCollisionPointRec(Mouse, GoToPageInputBox[4].Box)) boxColor = YELLOW; // Hover effect

            DrawRectangleRec(GoToPageInputBox[4].Box, boxColor);
            DrawTextEx(GIFont, GoToPageInputBox[4].Text, (Vector2){GoToPageInputBox[4].Box.x + 5, GoToPageInputBox[4].Box.y + 5}, FONT_H1, SPACING_WIDER, BLACK);

            if (GoToPageInputBox[4].IsActive && (FrameCounter / CURSOR_BLINK_SPEED) % 2 == 0) {
                int textWidth = MeasureTextEx(GIFont, GoToPageInputBox[4].Text, FONT_H1, SPACING_WIDER).x;
                DrawTextEx(GIFont, "|", (Vector2){GoToPageInputBox[4].Box.x + 5 + textWidth, GoToPageInputBox[4].Box.y + 5}, FONT_H1, SPACING_WIDER, BLACK);
            } if (strlen(GoToPageInputBox[4].Text) == 0 && !GoToPageInputBox[4].IsActive) {
                DrawTextEx(GIFont, "01", (Vector2){GoToPageInputBox[4].Box.x + 5, GoToPageInputBox[4].Box.y + 5}, FONT_H1, SPACING_WIDER, Fade(BLACK, 0.6));
            }
        }

        {
            if (CheckCollisionPointRec(Mouse, GOTOPAGE_Buttons)) {
                HoverTransition[5 + 930] = CustomLerp(HoverTransition[5 + 930], 1.0f, 0.1f);

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {                
                    int RequestedPage = atoi(GoToPageInputBox[4].Text);
                    if (RequestedPage < 1)  { RequestedPage = 1;  }
                    if (RequestedPage > 26) { RequestedPage = 26; }

                    int MaxAvailablePage = (ReadOnlyAffiliatedSchools + DISPLAY_PER_PAGE - 1) / DISPLAY_PER_PAGE; // Ensures proper rounding
                    if (RequestedPage > MaxAvailablePage) { RequestedPage = MaxAvailablePage; }

                    snprintf(GoToPageInputBox[4].Text, sizeof(GoToPageInputBox[4].Text), "%02d", RequestedPage);
                    CurrentPages[4] = RequestedPage - 1;

                    PlaySound(ButtonClickSFX);
                    Transitioning = true;
                    ScreenFade = 1.0f;
                    GoingBack = true;
                }
            } else HoverTransition[5 + 930] = CustomLerp(HoverTransition[5 + 930], 0.0f, 0.1f);
            
            GOTOPAGE_ButtonColor = (HoverTransition[5 + 930] > 0.5f) ? SKYBLUE : DARKBLUE;
            GOTOPAGE_TextColor = (HoverTransition[5 + 930] > 0.5f) ? BLACK : WHITE;
                
            float scale = 1.0f + HoverTransition[5 + 930] * 0.1f;
            float newWidth = GOTOPAGE_Buttons.width * scale;
            float newHeight = GOTOPAGE_Buttons.height * scale;
            float newX = GOTOPAGE_Buttons.x - (newWidth - GOTOPAGE_Buttons.width) / 2;
            float newY = GOTOPAGE_Buttons.y - (newHeight - GOTOPAGE_Buttons.height) / 2;
            
            DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, GOTOPAGE_ButtonColor);

            Vector2 textWidth = MeasureTextEx(GIFont, GOTOPAGE_ButtonTexts, (FONT_H3 * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_H3 * scale / 2);
            DrawTextEx(GIFont, GOTOPAGE_ButtonTexts, (Vector2){textX, textY}, (FONT_H3 * scale), SPACING_WIDER, GOTOPAGE_TextColor);
        }

        DrawManageViewBorder((OnlyAffiliatedSchoolsOnPage < 5) ? OnlyAffiliatedSchoolsOnPage : 4);
        for (int i = 0; i < OnlyAffiliatedSchoolsOnPage; i++) {
            snprintf(SAD_No, sizeof(SAD_No), "%03d", (i + 1) + (CurrentPages[4] * DISPLAY_PER_PAGE));
            SAD_Numberings[i].TextFill = SAD_No;
            SAD_Numberings[i].TextPosition = (Vector2){60 + LeftMargin, 240 + (144 * i) + ((TopMargin * 5) / 2) + 3};
            SAD_Numberings[i].FontData = (Vector2){FONT_H2, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_Numberings[i].TextFill, SAD_Numberings[i].TextPosition, SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y, SKYBLUE);

            SAD_FirstLine[i].TextFill = "Nama/Kode Vendor (atau Catering):";
            SAD_FirstLine[i].TextPosition = (Vector2){
                (60 + LeftMargin) + (MeasureTextEx(GIFont, "000", SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y).x + LeftMargin),
                240 + (144 * i) + (TopMargin / 2)
            };
            SAD_FirstLine[i].FontData = (Vector2){FONT_H5, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_FirstLine[i].TextFill, SAD_FirstLine[i].TextPosition, SAD_FirstLine[i].FontData.x, SAD_FirstLine[i].FontData.y, WHITE);

            SAD_VendorName[i].TextFill = OnlyAffiliatedSchools[i].AffiliatedVendor;
            SAD_VendorName[i].TextPosition = (Vector2){
                SAD_FirstLine[i].TextPosition.x,
                SAD_FirstLine[i].TextPosition.y + MeasureTextEx(GIFont, SAD_Numberings[i].TextFill, SAD_Numberings[i].FontData.x, SAD_Numberings[i].FontData.y).y - (TopMargin / 3)
            };
            SAD_VendorName[i].FontData = (Vector2){(strlen(SAD_VendorName[i].TextFill) <= 20) ? FONT_H1 : FONT_H2, SPACING_WIDER};
            SAD_VendorName[i].TextPosition.y += (SAD_VendorName[i].FontData.x == FONT_H1) ? 0 : 7;
            DrawTextEx(GIFont, SAD_VendorName[i].TextFill, SAD_VendorName[i].TextPosition, SAD_VendorName[i].FontData.x, SAD_VendorName[i].FontData.y, PURPLE);

            for (int c = 0; c < MAX_INPUT_LENGTH; c++) { SAD_GeneratedSecondLineMeasurement[c] = 'O'; }
            SAD_SecondLine[i].TextFill = "Ter-afiliasi dengan Sekolah:";
            SAD_SecondLine[i].TextPosition = (Vector2){
                (((60 + LeftMargin) + SAD_FirstLine[i].TextPosition.x) * 1.5) + MeasureTextEx(GIFont, SAD_GeneratedSecondLineMeasurement, SAD_FirstLine[i].FontData.x, SAD_FirstLine[i].FontData.y).x,
                240 + (144 * i) + (TopMargin / 2)
            };
            SAD_SecondLine[i].FontData = (Vector2){FONT_H5, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_SecondLine[i].TextFill, SAD_SecondLine[i].TextPosition, SAD_SecondLine[i].FontData.x, SAD_SecondLine[i].FontData.y, WHITE);
            SAD_GeneratedSecondLineMeasurement[0] = '\0';

            if (strlen(OnlyAffiliatedSchools[i].Name) == 0) SAD_SchoolName[i].TextFill = "-";
            else SAD_SchoolName[i].TextFill = OnlyAffiliatedSchools[i].Name;
            SAD_SchoolName[i].TextPosition = (Vector2){
                SAD_SecondLine[i].TextPosition.x,
                SAD_SecondLine[i].TextPosition.y + MeasureTextEx(GIFont, SAD_SecondLine[i].TextFill, SAD_SecondLine[i].FontData.x, SAD_SecondLine[i].FontData.y).y + (TopMargin + 5)
            };
            SAD_SchoolName[i].FontData = (Vector2){(strlen(SAD_SchoolName[i].TextFill) <= 20) ? FONT_H1 : FONT_H2, SPACING_WIDER};
            SAD_SchoolName[i].TextPosition.y -= (SAD_SchoolName[i].FontData.x == FONT_H1) ? 12 : 6;
            DrawTextEx(GIFont, SAD_SchoolName[i].TextFill, SAD_SchoolName[i].TextPosition, SAD_SchoolName[i].FontData.x, SAD_SchoolName[i].FontData.y, YELLOW);

            SAD_ThirdLine[i].TextFill = "Banyak Murid:";
            SAD_ThirdLine[i].TextPosition = (Vector2){
                SAD_SecondLine[i].TextPosition.x,
                (278 + (144 * i) + TopMargin) + MeasureTextEx(GIFont, SAD_SchoolName[i].TextFill, SAD_SchoolName[i].FontData.x, SAD_SchoolName[i].FontData.y).y + (SAD_SchoolName[i].FontData.x == FONT_H1 ? 0 : 12)
            };
            SAD_ThirdLine[i].FontData = (Vector2){FONT_H5, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_ThirdLine[i].TextFill, SAD_ThirdLine[i].TextPosition, SAD_ThirdLine[i].FontData.x, SAD_ThirdLine[i].FontData.y, WHITE);

            if (strlen(OnlyAffiliatedSchools[i].Students) == 0) SAD_ManyStudents[i].TextFill = "...";
            else SAD_ManyStudents[i].TextFill = OnlyAffiliatedSchools[i].Students;
            SAD_ManyStudents[i].TextPosition = (Vector2){
                SAD_ThirdLine[i].TextPosition.x + MeasureTextEx(GIFont, SAD_ThirdLine[i].TextFill, SAD_ThirdLine[i].FontData.x, SAD_ThirdLine[i].FontData.y).x + 10,
                SAD_ThirdLine[i].TextPosition.y - 2.38
            };
            SAD_ManyStudents[i].FontData = (Vector2){FONT_H4, SPACING_WIDER};
            DrawTextEx(GIFont, SAD_ManyStudents[i].TextFill, SAD_ManyStudents[i].TextPosition, SAD_ManyStudents[i].FontData.x, SAD_ManyStudents[i].FontData.y, BEIGE);
        }

        if (IsKeyPressed(KEY_RIGHT) && OnlyAffiliatedSchoolsOnPage == DISPLAY_PER_PAGE && (CurrentPages[4] * DISPLAY_PER_PAGE) + OnlyAffiliatedSchoolsOnPage < ReadOnlyAffiliatedSchools) { CurrentPages[4]++; }
        if (IsKeyPressed(KEY_LEFT) && CurrentPages[4] > 0) { CurrentPages[4]--; }
        
        DrawTextEx(GIFont, ShowRegisteredSchools.TextFill, ShowRegisteredSchools.TextPosition, ShowRegisteredSchools.FontData.x, ShowRegisteredSchools.FontData.y, PINK);
        DrawTextEx(GIFont, AffiliationMessage.TextFill, AffiliationMessage.TextPosition, AffiliationMessage.FontData.x, AffiliationMessage.FontData.y, ORANGE);
        DrawTextEx(GIFont, GoToPageGuide.TextFill, GoToPageGuide.TextPosition, GoToPageGuide.FontData.x, GoToPageGuide.FontData.y, WHITE);
        DrawTextEx(GIFont, PaginationInfo.TextFill, PaginationInfo.TextPosition, PaginationInfo.FontData.x, PaginationInfo.FontData.y, WHITE);
        DrawTextEx(GIFont, PaginationGuide.TextFill, PaginationGuide.TextPosition, PaginationGuide.FontData.x, PaginationGuide.FontData.y, GREEN);
    }
}

// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------

void VendorMainMenu(void) {
    char ReturnMenuState[256] = { 0 }, SetDayTM[256] = { 0 };
    int MainVendorButtonsInitialPosition = 320, MainVendorButtonsGap = 60;
    Vector2 MainVendorButtonsSize[2] = { (Vector2){680, 350}, (Vector2){((MainVendorButtonsSize[0].x * 2) - MainVendorButtonsGap) / 3, MainVendorButtonsSize[0].y / 1.5} };

    if (GetTimeOfDay() == 1)        snprintf(SetDayTM, sizeof(SetDayTM), "%s %s...", TEXT_MENU_MAIN_VENDOR_MORNING, User.Vendor.Username);
    else if (GetTimeOfDay() == 2)   snprintf(SetDayTM, sizeof(SetDayTM), "%s %s...", TEXT_MENU_MAIN_VENDOR_AFTERNOON, User.Vendor.Username);
    else if (GetTimeOfDay() == 3)   snprintf(SetDayTM, sizeof(SetDayTM), "%s %s...", TEXT_MENU_MAIN_VENDOR_EVENING, User.Vendor.Username);
    else if (GetTimeOfDay() == 4)   snprintf(SetDayTM, sizeof(SetDayTM), "%s %s...", TEXT_MENU_MAIN_VENDOR_NIGHT, User.Vendor.Username);
    GetMenuState(CurrentMenu, ReturnMenuState, true);

    TextObject TitleMessage = CreateTextObject(GIFont, SetDayTM, (strlen(User.Vendor.Username) < 20) ? FONT_H1 : FONT_H2, SPACING_WIDER, (Vector2){0, 100}, 20, true);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H3, SPACING_WIDER, (Vector2){0, TitleMessage.TextPosition.y}, 60, true);

    DrawSignOutButton();

    Rectangle FEATURE01__Button = {(SCREEN_WIDTH / 2) - MainVendorButtonsSize[0].x - (MainVendorButtonsGap / 2), MainVendorButtonsInitialPosition, (MainVendorButtonsSize[0].x * 2) + MainVendorButtonsGap, MainVendorButtonsSize[0].y};
    Rectangle FEATURE02__Button = {(SCREEN_WIDTH / 2) - MainVendorButtonsSize[0].x - (MainVendorButtonsGap / 2), (FEATURE01__Button.y + MainVendorButtonsSize[0].y) + MainVendorButtonsGap, MainVendorButtonsSize[1].x, MainVendorButtonsSize[1].y};
    Rectangle FEATURE03__Button = {FEATURE02__Button.x + MainVendorButtonsSize[1].x + MainVendorButtonsGap, (FEATURE01__Button.y + MainVendorButtonsSize[0].y) + MainVendorButtonsGap, MainVendorButtonsSize[1].x, MainVendorButtonsSize[1].y};
    Rectangle FEATURE04__Button = {FEATURE03__Button.x + MainVendorButtonsSize[1].x + MainVendorButtonsGap, (FEATURE01__Button.y + MainVendorButtonsSize[0].y) + MainVendorButtonsGap, MainVendorButtonsSize[1].x, MainVendorButtonsSize[1].y};
    
    Rectangle Buttons[4] = {FEATURE01__Button, FEATURE02__Button, FEATURE03__Button, FEATURE04__Button};
    Color ButtonColor[4] = { 0 }, TextColor[4] = { 0 };
    const char *ButtonTexts[] = {
        "~ [MANAGE]: Menu Program D'Magis Vendor ~",
        "~ [VIEW]: Afiliasi Sekolah ~",
        "~ [REQUEST]: Pengajuan RAB ~",
        "~ [VIEW]: Status Menu ~",
        "Fitur ini memungkinka Anda untuk mengatur LIMA (5) menu harian Anda sebagai Vendor (atau Catering), di mana menu-\nmenu tersebutlah yang akan menjadi menu M.S.G. bagi Sekolah yang telah ter-afiliasikan kepada Anda melalui\nPemerintah terkait.\n\nTerdapat opsi untuk MENAMBAHKAN menu baru (apabila warna kartu menu masih berwarna ABU-ABU CERAH), lalu opsi\nuntuk MENGUBAH isi menu yang sudah ada (selama menu terkait BELUM DISETUJUI pengajuannya oleh Pemerintah), dan\nopsi untuk MENGHAPUS isi menu apabila ingin mengganti isi menu yang sebelumnya sudah DISETUJUI oleh Pemerintah.\nCATATAN: Menu M.S.G. yang ditetapkan adalah \"4 SEHAT 5 SEMPURNA\".",
        "Lihat data Sekolah yang ter-afiliasi\ndengan Anda dari Pemerintah\nbeserta lampiran isi menu pada hari\nyang bersesuaian.",
        "Ajukan permintaan untuk\nditerimanya menu M.S.G. Anda\nkepada Pemerintah (berlaku untuk\nsemua kartu menu yang tersedia).",
        "Lihat semua status menu M.S.G.\nAnda yang telah diberi indikator\npesan sistem dan warna yang tepat.",
    };
    
    // Always updates the concurrent data of Vendors and Schools whenever changes or no changes are detected.
    // Again, just for safety reasons...
    // ReadUserAccount(Vendors);
    // ReadSchoolsData(Schools);
    // for (int i = 0; i < MAX_ACCOUNT_LIMIT; i++) {
    //     for (int j = 0; j < MAX_ACCOUNT_LIMIT; j++) {
    //         if (OriginalVendors[i].SaveSchoolDataIndex == j) {
    //             if (strcmp(OriginalVendors[i].AffiliatedSchoolData.Name, OriginalSchools[j].Name) != 0) { strcpy(OriginalVendors[i].AffiliatedSchoolData.Name, OriginalSchools[j].Name); }
    //             if (strcmp(OriginalVendors[i].AffiliatedSchoolData.Students, OriginalSchools[j].Students) != 0) { strcpy(OriginalVendors[i].AffiliatedSchoolData.Students, OriginalSchools[j].Students); }
    //         }
    //     }
    // }
    // WriteUserAccount(OriginalVendors);
    // SyncVendorsFromOriginalVendorData(Vendors, OriginalVendors);
    // Always updates the concurrent data of Vendors and Schools whenever changes or no changes are detected.
    // Again, just for safety reasons...

    Vector2 Mouse = GetMousePosition();
    DrawTextEx(GIFont, TitleMessage.TextFill, TitleMessage.TextPosition, TitleMessage.FontData.x, TitleMessage.FontData.y, WHITE);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

    // Always reset the menu profile cards...
    IsCardTransitioning = false;
    CurrentCardMenuProfile = 0;
    SetCardPositionInX = 0; // Start off-screen left
    TargetX = 0;
    TransitionDirection = 0; // 1 = right, -1 = left
    // Always reset the menu profile cards...

    for (int i = 21; i < 25; i++) {
        if (CheckCollisionPointRec(Mouse, Buttons[i - 21])) {
            HoverTransition[i] = CustomLerp(HoverTransition[i], 1.0f, 0.1f);
            
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (i == 21) NextMenu = MENU_MAIN_VENDOR_MenusManagement;
                if (i == 22) NextMenu = MENU_MAIN_VENDOR_GetAffiliatedSchoolData;
                if (i == 23) NextMenu = MENU_MAIN_VENDOR_SubmitBudgetPlan;
                if (i == 24) NextMenu = MENU_MAIN_VENDOR_GetDailyMenuStatusList;
                
                PlaySound(ButtonClickSFX);
                Transitioning = true;
                ScreenFade = 1.0f;
            }

        } else HoverTransition[i] = CustomLerp(HoverTransition[i], 0.0f, 0.1f);
        
        if (i == 21) {
            ButtonColor[i - 21] = (HoverTransition[i] > 0.5f) ? PURPLE : DARKPURPLE;
            TextColor[i - 21] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        } else if (i == 22) {
            ButtonColor[i - 21] = (HoverTransition[i] > 0.5f) ? SKYBLUE : DARKBLUE;
            TextColor[i - 21] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        } else if (i == 23) {
            ButtonColor[i - 21] = (HoverTransition[i] > 0.5f) ? GREEN : DARKGREEN;
            TextColor[i - 21] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        } else if (i == 24) {
            ButtonColor[i - 21] = (HoverTransition[i] > 0.5f) ? PINK : MAROON;
            TextColor[i - 21] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        }

        float scale = 1.0f + HoverTransition[i] * 0.1f;
        
        float newWidth = Buttons[i - 21].width * scale;
        float newHeight = Buttons[i - 21].height * scale;
        float newX = Buttons[i - 21].x - (newWidth - Buttons[i - 21].width) / 2;
        float newY = Buttons[i - 21].y - (newHeight - Buttons[i - 21].height) / 2;
        
        DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, ButtonColor[i - 21]);
        
        if (i == 21) {
            Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[(i - 21)], (FONT_H2 * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_H2 * scale / 2);
            DrawTextEx(GIFont, ButtonTexts[(i - 21)], (Vector2){textX, (textY - (textY / 4))}, (FONT_H2 * scale), SPACING_WIDER, TextColor[i - 21]);

            textWidth = MeasureTextEx(GIFont, ButtonTexts[(i - 21) + 4], (FONT_H5 * scale), SPACING_WIDER);
            textX = newX + (newWidth / 2) - (textWidth.x / 2);
            textY = newY + (newHeight / 2) - (FONT_H5 * scale / 2);
            DrawTextEx(GIFont, ButtonTexts[(i - 21) + 4], (Vector2){textX, (textY + 75) - (textY / 4)}, (FONT_H5 * scale), SPACING_WIDER, TextColor[i - 21]);
        } else {
            Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[(i - 21)], (FONT_P * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
            DrawTextEx(GIFont, ButtonTexts[(i - 21)], (Vector2){textX, (textY) - (textY / 12)}, (FONT_P * scale), SPACING_WIDER, TextColor[i - 21]);

            textWidth = MeasureTextEx(GIFont, ButtonTexts[(i - 21) + 4], (FONT_H5 * scale), SPACING_WIDER);
            textX = newX + (newWidth / 2) - (textWidth.x / 2);
            textY = newY + (newHeight / 2) - (FONT_H5 * scale / 2);
            DrawTextEx(GIFont, ButtonTexts[(i - 21) + 4], (Vector2){textX, (textY + 45) - (textY / 12)}, (FONT_H5 * scale), SPACING_WIDER, TextColor[i - 21]);
        }
    }
}

void VendorManageDailyMenusMenu(void) {
    char ReturnMenuState[256] = { 0 };

    PreviousMenu = MENU_MAIN_VENDOR;
    DrawPreviousMenuButton();

    GetMenuState(CurrentMenu, ReturnMenuState, false);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H2, SPACING_WIDER, (Vector2){0, 60}, 0, true);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

    Vector2 Mouse = GetMousePosition();

    // Resetting the error and success messages first...
    HasClicked[7] = false; HasClicked[8] = false;
    MenusManagementInputBox_ADD[0].Text[0] = '\0'; MenusManagementInputBox_ADD[1].Text[0] = '\0'; MenusManagementInputBox_ADD[2].Text[0] = '\0'; MenusManagementInputBox_ADD[3].Text[0] = '\0'; MenusManagementInputBox_ADD[4].Text[0] = '\0';
    MenusManagementInputBox_ADD_AllValid = true;
    MenusManagementInputBox_EDIT_AllValid = true;
    // Resetting the error and success messages first...

    if (!IsCardTransitioning) {
        // if (CheckCollisionPointRec(mouse, nextButton) && currentProfile < MAX_PROFILES - 1) {
        if (IsKeyPressed(KEY_RIGHT) && CurrentCardMenuProfile < (SCHOOL_DAYS - 1)) {
            TransitionDirection = 1; // Move right
            IsCardTransitioning = true;
            TargetX = SCREEN_WIDTH; // Move to the right
        }

        // if (CheckCollisionPointRec(mouse, prevButton) && currentProfile > 0) {
        if (IsKeyPressed(KEY_LEFT) && CurrentCardMenuProfile > 0) {
            TransitionDirection = -1; // Move left
            IsCardTransitioning = true;
            TargetX = -SCREEN_WIDTH; // Move to the left
        }
    }

    Rectangle NavigationButtons[2] = {
        {75, (SCREEN_HEIGHT / 2) - (210 / 2), 90, 210},
        {(SCREEN_WIDTH - 75) - 90, (SCREEN_HEIGHT / 2) - (210 / 2), 90, 210},
    };
    Color NavigationButtonColor[2], NavigationTextColor[2];
    const char *NavigationButtonsTexts[] = {"[<]", "[>]"};

    for (int i = 801; i < 803; i++) {
        if (CheckCollisionPointRec(Mouse, NavigationButtons[i - 801])) {
            HoverTransition[i] = CustomLerp(HoverTransition[i], 1.0f, 0.1f);
            
            if (!IsCardTransitioning) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    PlaySound(ButtonClickSFX);
                    if (i == 802 && CurrentCardMenuProfile < (SCHOOL_DAYS - 1)) {
                        TransitionDirection = 1; // Move right
                        IsCardTransitioning = true;
                        TargetX = SCREEN_WIDTH; // Move to the right
                    }

                    if (i == 801 && CurrentCardMenuProfile > 0) {
                        TransitionDirection = -1; // Move left
                        IsCardTransitioning = true;
                        TargetX = -SCREEN_WIDTH; // Move to the left
                    }
                }

            }

        } else HoverTransition[i] = CustomLerp(HoverTransition[i], 0.0f, 0.1f);
        
        if (i == 801) {
            NavigationButtonColor[i - 801] = (HoverTransition[i] > 0.5f) ? PURPLE : GRAY;
            NavigationTextColor[i - 801] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        } else if (i == 802) {
            NavigationButtonColor[i - 801] = (HoverTransition[i] > 0.5f) ? SKYBLUE : GRAY;
            NavigationTextColor[i - 801] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        }

        float scale = 1.0f + HoverTransition[i] * 0.1f;
        
        float newWidth = NavigationButtons[i - 801].width * scale;
        float newHeight = NavigationButtons[i - 801].height * scale;
        float newX = NavigationButtons[i - 801].x - (newWidth - NavigationButtons[i - 801].width) / 2;
        float newY = NavigationButtons[i - 801].y - (newHeight - NavigationButtons[i - 801].height) / 2;
        
        DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, NavigationButtonColor[i - 801]);
        
        Vector2 textWidth = MeasureTextEx(GIFont, NavigationButtonsTexts[(i - 801)], (FONT_H1 * scale), SPACING_WIDER);
        int textX = newX + (newWidth / 2) - (textWidth.x / 2);
        int textY = newY + (newHeight / 2) - (FONT_H1 * scale / 2);
        DrawTextEx(GIFont, NavigationButtonsTexts[(i - 801)], (Vector2){textX, textY}, (FONT_H1 * scale), SPACING_WIDER, NavigationTextColor[i - 801]);
    }

    // LERP Animation for Smooth Transition
    if (IsCardTransitioning) {
        SetCardPositionInX = CustomLerp(SetCardPositionInX, TargetX, 0.1f);

        // Stop transition when close enough
        if (fabs(SetCardPositionInX - TargetX) < 1.0f) {
            IsCardTransitioning = false;
            CurrentCardMenuProfile += TransitionDirection;
            SetCardPositionInX = 0; // Reset position
            TargetX = 0;
        }
    }

    // Draw IsCardTransitioning Profiles
    DrawMenuProfileCard(User.VendorID, CurrentCardMenuProfile, SetCardPositionInX, false, false, false);
    if (IsCardTransitioning) {
        DrawMenuProfileCard(User.VendorID, CurrentCardMenuProfile + TransitionDirection, SetCardPositionInX + (-TransitionDirection * SCREEN_WIDTH), false, false, false);
    }
}

void VendorManageDailyMenusMenu_ADD(void) {
    char ReturnMenuState[256] = { 0 };
    int ResetSubmitButtonGap = 40, ButtonsGap = 80;
    Vector2 SchoolDataInputBoxSize = {900, 60};
    Vector2 ResetSubmitButtonSize = {(SchoolDataInputBoxSize.x / 2) - (ButtonsGap / 4), 60};

    PreviousMenu = MENU_MAIN_VENDOR_MenusManagement;
    DrawPreviousMenuButton();

    GetMenuState(CurrentMenu, ReturnMenuState, false);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H2, SPACING_WIDER, (Vector2){0, 60}, 0, true);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

    const char *DataTexts[] = {
        "Silakan untuk MENAMBAHKAN data Menu M.S.G. untuk Program D'Magis Anda sebagai pihak Vendor (atau Catering)!",
        "Harap pastikan untuk setiap masukkan dari Anda telah menyesuaikan permintaan dari setiap kategori menu-nya...",

        "Makanan Pokok (cth. Nasi Putih)",
        "Lauk-pauk (cth. Ayam Goreng Serundeng)",
        "Sayur-sayuran (cth. Tumis Kangkung)",
        "Buah-buahan (cth. 1/8 Potong Semangka)",
        "Susu (cth. Susu Vanilla)",
        
        "Rentang: 1 s/d 30 Karakter",
        
        "Sukses! Anda telah berhasil MENAMBAHKAN Menu M.S.G. baru Anda...",

        "Gagal! Terdapat masukkan data Menu M.S.G. yang masih belum memenuhi kriteria...",
    };
    const char *ButtonTexts[2] = {"Reset...", "Tambahkan..."};
    
    Rectangle ResetButton = {(SCREEN_WIDTH / 2) - ResetSubmitButtonSize.x - (ResetSubmitButtonGap / 2), MenusManagementInputBox_ADD[4].Box.y + ButtonsGap, ResetSubmitButtonSize.x, ResetSubmitButtonSize.y};
    Rectangle SubmitButton = {ResetButton.x + ResetSubmitButtonSize.x + ResetSubmitButtonGap, ResetButton.y, ResetSubmitButtonSize.x, ResetSubmitButtonSize.y};
    Rectangle Buttons[2] = {ResetButton, SubmitButton};
    Color ButtonColor[2] = { 0 }, TextColor[2] = { 0 };

    char PrepareMenuInputMessage[64] = { 0 }, PrepareEmptyMenuInputMessage[128] = { 0 };
    snprintf(PrepareMenuInputMessage, sizeof(PrepareMenuInputMessage), "Pengisian Menu M.S.G. pada hari: %s.", (VendorMenuIndex == 0) ? "SENIN" : (VendorMenuIndex == 1) ? "SELASA" : (VendorMenuIndex == 2) ? "RABU" : (VendorMenuIndex == 3) ? "KAMIS" : "JUM'AT");
    TextObject MenuInputMessage = CreateTextObject(GIFont, PrepareMenuInputMessage, FONT_H5, SPACING_WIDER, (Vector2){60, 980}, 0, false);
    snprintf(PrepareEmptyMenuInputMessage, sizeof(PrepareEmptyMenuInputMessage), "Ketik tanda \"-\" (tanpa tanda petiknya) sebagai indikasi kategori menu tersebut adalah KOSONG.");
    TextObject EmptyMenuInputMessage = CreateTextObject(GIFont, PrepareEmptyMenuInputMessage, FONT_H5, SPACING_WIDER, (Vector2){(SCREEN_WIDTH - 60) - MeasureTextEx(GIFont, PrepareEmptyMenuInputMessage, FONT_H5, SPACING_WIDER).x, 980}, 0, false);

    TextObject InputBoxHelp[5] = {
        CreateTextObject(GIFont, DataTexts[2], (SchoolDataInputBoxSize.y / 2), SPACING_WIDER, (Vector2){MenusManagementInputBox_ADD[0].Box.x + 10, MenusManagementInputBox_ADD[0].Box.y + 15}, 0, false),
        CreateTextObject(GIFont, DataTexts[3], (SchoolDataInputBoxSize.y / 2), SPACING_WIDER, (Vector2){MenusManagementInputBox_ADD[1].Box.x + 10, MenusManagementInputBox_ADD[1].Box.y + 15}, 0, false),
        CreateTextObject(GIFont, DataTexts[4], (SchoolDataInputBoxSize.y / 2), SPACING_WIDER, (Vector2){MenusManagementInputBox_ADD[2].Box.x + 10, MenusManagementInputBox_ADD[2].Box.y + 15}, 0, false),
        CreateTextObject(GIFont, DataTexts[5], (SchoolDataInputBoxSize.y / 2), SPACING_WIDER, (Vector2){MenusManagementInputBox_ADD[3].Box.x + 10, MenusManagementInputBox_ADD[3].Box.y + 15}, 0, false),
        CreateTextObject(GIFont, DataTexts[6], (SchoolDataInputBoxSize.y / 2), SPACING_WIDER, (Vector2){MenusManagementInputBox_ADD[4].Box.x + 10, MenusManagementInputBox_ADD[4].Box.y + 15}, 0, false)
    };
    TextObject MaxCharsInfo[5] = {
        CreateTextObject(GIFont, DataTexts[7], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER, (Vector2){MenusManagementInputBox_ADD[0].Box.x + MenusManagementInputBox_ADD[0].Box.width - (MeasureTextEx(GIFont, DataTexts[7], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER).x) - 15, MenusManagementInputBox_ADD[0].Box.y + (SchoolDataInputBoxSize.y / 3)}, 0, false),
        CreateTextObject(GIFont, DataTexts[7], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER, (Vector2){MenusManagementInputBox_ADD[1].Box.x + MenusManagementInputBox_ADD[1].Box.width - (MeasureTextEx(GIFont, DataTexts[7], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER).x) - 15, MenusManagementInputBox_ADD[1].Box.y + (SchoolDataInputBoxSize.y / 3)}, 0, false),
        CreateTextObject(GIFont, DataTexts[7], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER, (Vector2){MenusManagementInputBox_ADD[2].Box.x + MenusManagementInputBox_ADD[2].Box.width - (MeasureTextEx(GIFont, DataTexts[7], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER).x) - 15, MenusManagementInputBox_ADD[2].Box.y + (SchoolDataInputBoxSize.y / 3)}, 0, false),
        CreateTextObject(GIFont, DataTexts[7], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER, (Vector2){MenusManagementInputBox_ADD[3].Box.x + MenusManagementInputBox_ADD[3].Box.width - (MeasureTextEx(GIFont, DataTexts[7], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER).x) - 15, MenusManagementInputBox_ADD[3].Box.y + (SchoolDataInputBoxSize.y / 3)}, 0, false),
        CreateTextObject(GIFont, DataTexts[7], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER, (Vector2){MenusManagementInputBox_ADD[4].Box.x + MenusManagementInputBox_ADD[4].Box.width - (MeasureTextEx(GIFont, DataTexts[7], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER).x) - 15, MenusManagementInputBox_ADD[4].Box.y + (SchoolDataInputBoxSize.y / 3)}, 0, false)
    };

    TextObject AddMenuMessage[2] = {
        CreateTextObject(GIFont, DataTexts[0], FONT_H3, SPACING_WIDER, (Vector2){0, 240 + 20}, 0, true),
        CreateTextObject(GIFont, DataTexts[1], FONT_H3, SPACING_WIDER, AddMenuMessage[0].TextPosition, 40, true)
    };
    TextObject SuccessMessage = CreateTextObject(GIFont, DataTexts[8], FONT_H2, SPACING_WIDER, (Vector2){0, 180}, 0, true);
    TextObject FirstErrorMessage = CreateTextObject(GIFont, DataTexts[9], FONT_H2, SPACING_WIDER, SuccessMessage.TextPosition, 0, true);
    // TextObject HelpInfo = CreateTextObject(GIFont, DataTexts[8], FONT_H5, SPACING_WIDER, (Vector2){100, ResetButton.y}, 75 + ResetButton.height, false);
    
    MenusManagementInputBox_ADD[0].Box = (Rectangle){(SCREEN_WIDTH / 2) - (SchoolDataInputBoxSize.x / 2), 420, SchoolDataInputBoxSize.x, SchoolDataInputBoxSize.y};
    MenusManagementInputBox_ADD[1].Box = (Rectangle){(SCREEN_WIDTH / 2) - (SchoolDataInputBoxSize.x / 2), MenusManagementInputBox_ADD[0].Box.y + ButtonsGap, SchoolDataInputBoxSize.x, SchoolDataInputBoxSize.y};
    MenusManagementInputBox_ADD[2].Box = (Rectangle){(SCREEN_WIDTH / 2) - (SchoolDataInputBoxSize.x / 2), MenusManagementInputBox_ADD[1].Box.y + ButtonsGap, SchoolDataInputBoxSize.x, SchoolDataInputBoxSize.y};
    MenusManagementInputBox_ADD[3].Box = (Rectangle){(SCREEN_WIDTH / 2) - (SchoolDataInputBoxSize.x / 2), MenusManagementInputBox_ADD[2].Box.y + ButtonsGap, SchoolDataInputBoxSize.x, SchoolDataInputBoxSize.y};
    MenusManagementInputBox_ADD[4].Box = (Rectangle){(SCREEN_WIDTH / 2) - (SchoolDataInputBoxSize.x / 2), MenusManagementInputBox_ADD[3].Box.y + ButtonsGap, SchoolDataInputBoxSize.x, SchoolDataInputBoxSize.y};

    DrawManageViewBorder(0);
    if (HasClicked[7]) {
        if (MenusManagementInputBox_ADD_AllValid) {
            DrawTextEx(GIFont, FirstErrorMessage.TextFill, FirstErrorMessage.TextPosition, FirstErrorMessage.FontData.x, FirstErrorMessage.FontData.y, Fade(BLACK, 0.0f));
            DrawTextEx(GIFont, SuccessMessage.TextFill, SuccessMessage.TextPosition, SuccessMessage.FontData.x, SuccessMessage.FontData.y, GREEN);
        } else {
            DrawTextEx(GIFont, SuccessMessage.TextFill, SuccessMessage.TextPosition, SuccessMessage.FontData.x, SuccessMessage.FontData.y, Fade(BLACK, 0.0f));
            DrawTextEx(GIFont, FirstErrorMessage.TextFill, FirstErrorMessage.TextPosition, FirstErrorMessage.FontData.x, FirstErrorMessage.FontData.y, RED);
        }
    }

    for (int i = 0; i < 2; i++) { DrawTextEx(GIFont, AddMenuMessage[i].TextFill, AddMenuMessage[i].TextPosition, AddMenuMessage[i].FontData.x, AddMenuMessage[i].FontData.y, PINK); }
    for (int i = 0; i < 5; i++) { DrawRectangleRec(MenusManagementInputBox_ADD[1].Box, GRAY); }

    Vector2 Mouse = GetMousePosition();
    for (int i = 0; i < 2; i++) {
        if (CheckCollisionPointRec(Mouse, Buttons[i])) {
            HoverTransition[i + 901] = CustomLerp(HoverTransition[i + 901], 1.0f, 0.1f);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (i == 0) ResetInputs(MenusManagementInputBox_ADD, (Vector2){0, 5});
                if (i == 1) {
                    MenusManagementInputBox_ADD_AllValid = true;

                    for (int j = 0; j < 5; j++) {
                        MenusManagementInputBox_ADD[j].IsValid = (strlen(MenusManagementInputBox_ADD[j].Text) >= 1 && strlen(MenusManagementInputBox_ADD[j].Text) <= 30);
                        if (!MenusManagementInputBox_ADD[j].IsValid) { MenusManagementInputBox_ADD_AllValid = false; }
                    }

                    if (MenusManagementInputBox_ADD_AllValid) {
                        ReadUserAccount(Vendors);
                        strcpy(OriginalVendors[User.VendorID].Menus[VendorMenuIndex].Carbohydrate, MenusManagementInputBox_ADD[0].Text);
                        strcpy(OriginalVendors[User.VendorID].Menus[VendorMenuIndex].Protein, MenusManagementInputBox_ADD[1].Text);
                        strcpy(OriginalVendors[User.VendorID].Menus[VendorMenuIndex].Vegetables, MenusManagementInputBox_ADD[2].Text);
                        strcpy(OriginalVendors[User.VendorID].Menus[VendorMenuIndex].Fruits, MenusManagementInputBox_ADD[3].Text);
                        strcpy(OriginalVendors[User.VendorID].Menus[VendorMenuIndex].Milk, MenusManagementInputBox_ADD[4].Text);
                        
                        WriteUserAccount(OriginalVendors);
                        SyncVendorsFromOriginalVendorData(Vendors, OriginalVendors);
                    }
                    
                    HasClicked[7] = true;
                }
                
                PlaySound(ButtonClickSFX);
                Transitioning = true;
                ScreenFade = 1.0f;
            }
        } else HoverTransition[i + 901] = CustomLerp(HoverTransition[i + 901], 0.0f, 0.1f);
        
        if (i == 0) { // Reset button
            ButtonColor[i] = (HoverTransition[i + 901] > 0.5f) ? RED : LIGHTGRAY;
            TextColor[i] = (HoverTransition[i + 901] > 0.5f) ? WHITE : BLACK;
        } else if (i == 1) { // Submit button
            ButtonColor[i] = (HoverTransition[i + 901] > 0.5f) ? GREEN : LIGHTGRAY;
            TextColor[i] = (HoverTransition[i + 901] > 0.5f) ? WHITE : BLACK;
        }

        float scale = 1.0f + HoverTransition[i + 901] * 0.1f;
        
        float newWidth = Buttons[i].width * scale;
        float newHeight = Buttons[i].height * scale;
        float newX = Buttons[i].x - (newWidth - Buttons[i].width) / 2;
        float newY = Buttons[i].y - (newHeight - Buttons[i].height) / 2;
        
        DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, ButtonColor[i]);
        
        Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[i], (FONT_P * scale), SPACING_WIDER);
        int textX = newX + (newWidth / 2) - (textWidth.x / 2);
        int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
        DrawTextEx(GIFont, ButtonTexts[i], (Vector2){textX, textY}, (FONT_P * scale), SPACING_WIDER, TextColor[i]);
    }
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        ActiveBox = -1;  // Reset active box
        for (int i = 0; i < 5; i++) {
            if (CheckCollisionPointRec(Mouse, MenusManagementInputBox_ADD[i].Box)) {
                ActiveBox = i;
                break;
            }
        }
    }

    if (ActiveBox != -1) {
        int Key = GetCharPressed();
        while (Key > 0) {
            if (strlen(MenusManagementInputBox_ADD[ActiveBox].Text) < MAX_INPUT_LENGTH) {
                int len = strlen(MenusManagementInputBox_ADD[ActiveBox].Text);
                MenusManagementInputBox_ADD[ActiveBox].Text[len] = (char)Key;
                MenusManagementInputBox_ADD[ActiveBox].Text[len + 1] = '\0';
            } Key = GetCharPressed();
        }

        if (IsKeyPressed(KEY_BACKSPACE) && strlen(MenusManagementInputBox_ADD[ActiveBox].Text) > 0) {
            MenusManagementInputBox_ADD[ActiveBox].Text[strlen(MenusManagementInputBox_ADD[ActiveBox].Text) - 1] = '\0';
        } else if (IsKeyDown(KEY_BACKSPACE) && strlen(MenusManagementInputBox_ADD[ActiveBox].Text) > 0) {
            CurrentFrameTime += GetFrameTime();
            if (CurrentFrameTime >= 0.5f) {
                MenusManagementInputBox_ADD[ActiveBox].Text[strlen(MenusManagementInputBox_ADD[ActiveBox].Text) - 1] = '\0';
            }
        }
        
        if (IsKeyReleased(KEY_BACKSPACE)) CurrentFrameTime = 0;
    }

    FrameCounter++; // Increment frame counter for cursor blinking
    for (int i = 0; i < 5; i++) {
        Color boxColor = GRAY;
        if (!MenusManagementInputBox_ADD[i].IsValid && HasClicked[7]) { boxColor = RED; }
        
        if (i == ActiveBox) boxColor = SKYBLUE; // Active input
        else if (CheckCollisionPointRec(Mouse, MenusManagementInputBox_ADD[i].Box)) boxColor = YELLOW; // Hover effect

        DrawRectangleRec(MenusManagementInputBox_ADD[i].Box, boxColor);
        DrawTextEx(GIFont, MenusManagementInputBox_ADD[i].Text, (Vector2){MenusManagementInputBox_ADD[i].Box.x + 10, MenusManagementInputBox_ADD[i].Box.y + 15}, (SchoolDataInputBoxSize.y / 2), SPACING_WIDER, BLACK);

        if (i == ActiveBox && (FrameCounter / CURSOR_BLINK_SPEED) % 2 == 0) {
            int textWidth = MeasureTextEx(GIFont, MenusManagementInputBox_ADD[i].Text, (SchoolDataInputBoxSize.y / 2), SPACING_WIDER).x;
            DrawTextEx(GIFont, "|", (Vector2){MenusManagementInputBox_ADD[i].Box.x + 10 + textWidth, MenusManagementInputBox_ADD[i].Box.y + 10}, (SchoolDataInputBoxSize.y / 2) + 10, SPACING_WIDER, BLACK);
        }
        
        DrawTextEx(GIFont, MaxCharsInfo[i].TextFill, MaxCharsInfo[i].TextPosition, MaxCharsInfo[i].FontData.x, MaxCharsInfo[i].FontData.y, BLACK);
        if (strlen(MenusManagementInputBox_ADD[i].Text) == 0 && i != ActiveBox) {
            DrawTextEx(GIFont, InputBoxHelp[i].TextFill, InputBoxHelp[i].TextPosition, InputBoxHelp[i].FontData.x, InputBoxHelp[i].FontData.y, Fade(BLACK, 0.6));
        }
    }

    DrawTextEx(GIFont, MenuInputMessage.TextFill, MenuInputMessage.TextPosition, MenuInputMessage.FontData.x, MenuInputMessage.FontData.y, WHITE);
    DrawTextEx(GIFont, EmptyMenuInputMessage.TextFill, EmptyMenuInputMessage.TextPosition, EmptyMenuInputMessage.FontData.x, EmptyMenuInputMessage.FontData.y, GOLD);
}

void VendorManageDailyMenusMenu_EDIT(void) {
    char ReturnMenuState[256] = { 0 };
    int ResetSubmitButtonGap = 40, ButtonsGap = 80;
    Vector2 SchoolDataInputBoxSize = {900, 60};
    Vector2 ResetSubmitButtonSize = {(SchoolDataInputBoxSize.x / 2) - (ButtonsGap / 4), 60};

    PreviousMenu = MENU_MAIN_VENDOR_MenusManagement;
    DrawPreviousMenuButton();

    GetMenuState(CurrentMenu, ReturnMenuState, false);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H2, SPACING_WIDER, (Vector2){0, 60}, 0, true);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

    const char *DataTexts[] = {
        "Silakan untuk MENGUBAH data Menu M.S.G. untuk Program D'Magis Anda sebagai pihak Vendor (atau Catering)!",
        "Harap pastikan untuk setiap masukkan dari Anda telah menyesuaikan permintaan dari setiap kategori menu-nya...",

        "Makanan Pokok (cth. Nasi Putih)",
        "Lauk-pauk (cth. Ayam Goreng Serundeng)",
        "Sayur-sayuran (cth. Sayur Kangkung)",
        "Buah-buahan (cth. 1/8 Potong Semangka)",
        "Susu (cth. Susu Vanilla)",
        
        "Rentang: 1 s/d 30 Karakter",
        
        "Sukses! Anda telah berhasil MENGUBAH Menu M.S.G. Anda...",

        "Gagal! Terdapat masukkan data Menu M.S.G. yang masih belum memenuhi kriteria...",
    };
    const char *ButtonTexts[2] = {"Reset...", "Ubahkan..."};
    
    Rectangle ResetButton = {(SCREEN_WIDTH / 2) - ResetSubmitButtonSize.x - (ResetSubmitButtonGap / 2), MenusManagementInputBox_EDIT[4].Box.y + ButtonsGap, ResetSubmitButtonSize.x, ResetSubmitButtonSize.y};
    Rectangle SubmitButton = {ResetButton.x + ResetSubmitButtonSize.x + ResetSubmitButtonGap, ResetButton.y, ResetSubmitButtonSize.x, ResetSubmitButtonSize.y};
    Rectangle Buttons[2] = {ResetButton, SubmitButton};
    Color ButtonColor[2] = { 0 }, TextColor[2] = { 0 };

    char PrepareMenuInputMessage[64] = { 0 }, PrepareEmptyMenuInputMessage[128] = { 0 };
    snprintf(PrepareMenuInputMessage, sizeof(PrepareMenuInputMessage), "Pengisian Menu M.S.G. pada hari: %s.", (VendorMenuIndex == 0) ? "SENIN" : (VendorMenuIndex == 1) ? "SELASA" : (VendorMenuIndex == 2) ? "RABU" : (VendorMenuIndex == 3) ? "KAMIS" : "JUM'AT");
    TextObject MenuInputMessage = CreateTextObject(GIFont, PrepareMenuInputMessage, FONT_H5, SPACING_WIDER, (Vector2){60, 980}, 0, false);
    snprintf(PrepareEmptyMenuInputMessage, sizeof(PrepareEmptyMenuInputMessage), "Ketik tanda \"-\" (tanpa tanda petiknya) sebagai indikasi kategori menu tersebut adalah KOSONG.");
    TextObject EmptyMenuInputMessage = CreateTextObject(GIFont, PrepareEmptyMenuInputMessage, FONT_H5, SPACING_WIDER, (Vector2){(SCREEN_WIDTH - 60) - MeasureTextEx(GIFont, PrepareEmptyMenuInputMessage, FONT_H5, SPACING_WIDER).x, 980}, 0, false);

    TextObject InputBoxHelp[5] = {
        CreateTextObject(GIFont, DataTexts[2], (SchoolDataInputBoxSize.y / 2), SPACING_WIDER, (Vector2){MenusManagementInputBox_EDIT[0].Box.x + 10, MenusManagementInputBox_EDIT[0].Box.y + 15}, 0, false),
        CreateTextObject(GIFont, DataTexts[3], (SchoolDataInputBoxSize.y / 2), SPACING_WIDER, (Vector2){MenusManagementInputBox_EDIT[1].Box.x + 10, MenusManagementInputBox_EDIT[1].Box.y + 15}, 0, false),
        CreateTextObject(GIFont, DataTexts[4], (SchoolDataInputBoxSize.y / 2), SPACING_WIDER, (Vector2){MenusManagementInputBox_EDIT[2].Box.x + 10, MenusManagementInputBox_EDIT[2].Box.y + 15}, 0, false),
        CreateTextObject(GIFont, DataTexts[5], (SchoolDataInputBoxSize.y / 2), SPACING_WIDER, (Vector2){MenusManagementInputBox_EDIT[3].Box.x + 10, MenusManagementInputBox_EDIT[3].Box.y + 15}, 0, false),
        CreateTextObject(GIFont, DataTexts[6], (SchoolDataInputBoxSize.y / 2), SPACING_WIDER, (Vector2){MenusManagementInputBox_EDIT[4].Box.x + 10, MenusManagementInputBox_EDIT[4].Box.y + 15}, 0, false)
    };
    TextObject MaxCharsInfo[5] = {
        CreateTextObject(GIFont, DataTexts[7], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER, (Vector2){MenusManagementInputBox_EDIT[0].Box.x + MenusManagementInputBox_EDIT[0].Box.width - (MeasureTextEx(GIFont, DataTexts[7], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER).x) - 15, MenusManagementInputBox_EDIT[0].Box.y + (SchoolDataInputBoxSize.y / 3)}, 0, false),
        CreateTextObject(GIFont, DataTexts[7], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER, (Vector2){MenusManagementInputBox_EDIT[1].Box.x + MenusManagementInputBox_EDIT[1].Box.width - (MeasureTextEx(GIFont, DataTexts[7], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER).x) - 15, MenusManagementInputBox_EDIT[1].Box.y + (SchoolDataInputBoxSize.y / 3)}, 0, false),
        CreateTextObject(GIFont, DataTexts[7], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER, (Vector2){MenusManagementInputBox_EDIT[2].Box.x + MenusManagementInputBox_EDIT[2].Box.width - (MeasureTextEx(GIFont, DataTexts[7], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER).x) - 15, MenusManagementInputBox_EDIT[2].Box.y + (SchoolDataInputBoxSize.y / 3)}, 0, false),
        CreateTextObject(GIFont, DataTexts[7], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER, (Vector2){MenusManagementInputBox_EDIT[3].Box.x + MenusManagementInputBox_EDIT[3].Box.width - (MeasureTextEx(GIFont, DataTexts[7], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER).x) - 15, MenusManagementInputBox_EDIT[3].Box.y + (SchoolDataInputBoxSize.y / 3)}, 0, false),
        CreateTextObject(GIFont, DataTexts[7], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER, (Vector2){MenusManagementInputBox_EDIT[4].Box.x + MenusManagementInputBox_EDIT[4].Box.width - (MeasureTextEx(GIFont, DataTexts[7], (SchoolDataInputBoxSize.y / 3), SPACING_WIDER).x) - 15, MenusManagementInputBox_EDIT[4].Box.y + (SchoolDataInputBoxSize.y / 3)}, 0, false)
    };

    TextObject AddMenuMessage[2] = {
        CreateTextObject(GIFont, DataTexts[0], FONT_H3, SPACING_WIDER, (Vector2){0, 240 + 20}, 0, true),
        CreateTextObject(GIFont, DataTexts[1], FONT_H3, SPACING_WIDER, AddMenuMessage[0].TextPosition, 40, true)
    };
    TextObject SuccessMessage = CreateTextObject(GIFont, DataTexts[8], FONT_H2, SPACING_WIDER, (Vector2){0, 180}, 0, true);
    TextObject FirstErrorMessage = CreateTextObject(GIFont, DataTexts[9], FONT_H2, SPACING_WIDER, SuccessMessage.TextPosition, 0, true);
    // TextObject HelpInfo = CreateTextObject(GIFont, DataTexts[8], FONT_H5, SPACING_WIDER, (Vector2){100, ResetButton.y}, 75 + ResetButton.height, false);
    
    MenusManagementInputBox_EDIT[0].Box = (Rectangle){(SCREEN_WIDTH / 2) - (SchoolDataInputBoxSize.x / 2), 420, SchoolDataInputBoxSize.x, SchoolDataInputBoxSize.y};
    MenusManagementInputBox_EDIT[1].Box = (Rectangle){(SCREEN_WIDTH / 2) - (SchoolDataInputBoxSize.x / 2), MenusManagementInputBox_EDIT[0].Box.y + ButtonsGap, SchoolDataInputBoxSize.x, SchoolDataInputBoxSize.y};
    MenusManagementInputBox_EDIT[2].Box = (Rectangle){(SCREEN_WIDTH / 2) - (SchoolDataInputBoxSize.x / 2), MenusManagementInputBox_EDIT[1].Box.y + ButtonsGap, SchoolDataInputBoxSize.x, SchoolDataInputBoxSize.y};
    MenusManagementInputBox_EDIT[3].Box = (Rectangle){(SCREEN_WIDTH / 2) - (SchoolDataInputBoxSize.x / 2), MenusManagementInputBox_EDIT[2].Box.y + ButtonsGap, SchoolDataInputBoxSize.x, SchoolDataInputBoxSize.y};
    MenusManagementInputBox_EDIT[4].Box = (Rectangle){(SCREEN_WIDTH / 2) - (SchoolDataInputBoxSize.x / 2), MenusManagementInputBox_EDIT[3].Box.y + ButtonsGap, SchoolDataInputBoxSize.x, SchoolDataInputBoxSize.y};    

    DrawManageViewBorder(0);
    if (HasClicked[8]) {
        if (MenusManagementInputBox_EDIT_AllValid) {
            DrawTextEx(GIFont, FirstErrorMessage.TextFill, FirstErrorMessage.TextPosition, FirstErrorMessage.FontData.x, FirstErrorMessage.FontData.y, Fade(BLACK, 0.0f));
            DrawTextEx(GIFont, SuccessMessage.TextFill, SuccessMessage.TextPosition, SuccessMessage.FontData.x, SuccessMessage.FontData.y, GREEN);
        } else {
            DrawTextEx(GIFont, SuccessMessage.TextFill, SuccessMessage.TextPosition, SuccessMessage.FontData.x, SuccessMessage.FontData.y, Fade(BLACK, 0.0f));
            DrawTextEx(GIFont, FirstErrorMessage.TextFill, FirstErrorMessage.TextPosition, FirstErrorMessage.FontData.x, FirstErrorMessage.FontData.y, RED);
        }
    }
    
    for (int i = 0; i < 2; i++) { DrawTextEx(GIFont, AddMenuMessage[i].TextFill, AddMenuMessage[i].TextPosition, AddMenuMessage[i].FontData.x, AddMenuMessage[i].FontData.y, PINK); }
    for (int i = 0; i < 5; i++) { DrawRectangleRec(MenusManagementInputBox_EDIT[1].Box, GRAY); }

    Vector2 Mouse = GetMousePosition();
    for (int i = 0; i < 2; i++) {
        if (CheckCollisionPointRec(Mouse, Buttons[i])) {
            HoverTransition[i + 901] = CustomLerp(HoverTransition[i + 901], 1.0f, 0.1f);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (i == 0) ResetInputs(MenusManagementInputBox_EDIT, (Vector2){0, 5});
                if (i == 1) {
                    MenusManagementInputBox_EDIT_AllValid = true;

                    for (int j = 0; j < 5; j++) {
                        MenusManagementInputBox_EDIT[j].IsValid = (strlen(MenusManagementInputBox_EDIT[j].Text) >= 1 && strlen(MenusManagementInputBox_EDIT[j].Text) <= 30);
                        if (!MenusManagementInputBox_EDIT[j].IsValid) { MenusManagementInputBox_EDIT_AllValid = false; }
                    }

                    if (MenusManagementInputBox_EDIT_AllValid) {
                        ReadUserAccount(Vendors);
                        strcpy(OriginalVendors[User.VendorID].Menus[VendorMenuIndex].Carbohydrate, MenusManagementInputBox_EDIT[0].Text);
                        strcpy(OriginalVendors[User.VendorID].Menus[VendorMenuIndex].Protein, MenusManagementInputBox_EDIT[1].Text);
                        strcpy(OriginalVendors[User.VendorID].Menus[VendorMenuIndex].Vegetables, MenusManagementInputBox_EDIT[2].Text);
                        strcpy(OriginalVendors[User.VendorID].Menus[VendorMenuIndex].Fruits, MenusManagementInputBox_EDIT[3].Text);
                        strcpy(OriginalVendors[User.VendorID].Menus[VendorMenuIndex].Milk, MenusManagementInputBox_EDIT[4].Text);
                        
                        WriteUserAccount(OriginalVendors);
                        SyncVendorsFromOriginalVendorData(Vendors, OriginalVendors);
                    }
                        
                    HasClicked[8] = true;
                }
                
                PlaySound(ButtonClickSFX);
                Transitioning = true;
                ScreenFade = 1.0f;
            }
        } else HoverTransition[i + 901] = CustomLerp(HoverTransition[i + 901], 0.0f, 0.1f);
        
        if (i == 0) { // Reset button
            ButtonColor[i] = (HoverTransition[i + 901] > 0.5f) ? RED : LIGHTGRAY;
            TextColor[i] = (HoverTransition[i + 901] > 0.5f) ? WHITE : BLACK;
        } else if (i == 1) { // Submit button
            ButtonColor[i] = (HoverTransition[i + 901] > 0.5f) ? GREEN : LIGHTGRAY;
            TextColor[i] = (HoverTransition[i + 901] > 0.5f) ? WHITE : BLACK;
        }

        float scale = 1.0f + HoverTransition[i + 901] * 0.1f;
        
        float newWidth = Buttons[i].width * scale;
        float newHeight = Buttons[i].height * scale;
        float newX = Buttons[i].x - (newWidth - Buttons[i].width) / 2;
        float newY = Buttons[i].y - (newHeight - Buttons[i].height) / 2;
        
        DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, ButtonColor[i]);
        
        Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[i], (FONT_P * scale), SPACING_WIDER);
        int textX = newX + (newWidth / 2) - (textWidth.x / 2);
        int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
        DrawTextEx(GIFont, ButtonTexts[i], (Vector2){textX, textY}, (FONT_P * scale), SPACING_WIDER, TextColor[i]);
    }
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        ActiveBox = -1;  // Reset active box
        for (int i = 0; i < 5; i++) {
            if (CheckCollisionPointRec(Mouse, MenusManagementInputBox_EDIT[i].Box)) {
                ActiveBox = i;
                break;
            }
        }
    }

    if (ActiveBox != -1) {
        int Key = GetCharPressed();
        while (Key > 0) {
            if (strlen(MenusManagementInputBox_EDIT[ActiveBox].Text) < MAX_INPUT_LENGTH) {
                int len = strlen(MenusManagementInputBox_EDIT[ActiveBox].Text);
                MenusManagementInputBox_EDIT[ActiveBox].Text[len] = (char)Key;
                MenusManagementInputBox_EDIT[ActiveBox].Text[len + 1] = '\0';
            } Key = GetCharPressed();
        }

        if (IsKeyPressed(KEY_BACKSPACE) && strlen(MenusManagementInputBox_EDIT[ActiveBox].Text) > 0) {
            MenusManagementInputBox_EDIT[ActiveBox].Text[strlen(MenusManagementInputBox_EDIT[ActiveBox].Text) - 1] = '\0';
        } else if (IsKeyDown(KEY_BACKSPACE) && strlen(MenusManagementInputBox_EDIT[ActiveBox].Text) > 0) {
            CurrentFrameTime += GetFrameTime();
            if (CurrentFrameTime >= 0.5f) {
                MenusManagementInputBox_EDIT[ActiveBox].Text[strlen(MenusManagementInputBox_EDIT[ActiveBox].Text) - 1] = '\0';
            }
        }
        
        if (IsKeyReleased(KEY_BACKSPACE)) CurrentFrameTime = 0;
    }

    FrameCounter++; // Increment frame counter for cursor blinking
    for (int i = 0; i < 5; i++) {
        Color boxColor = GRAY;
        if (!MenusManagementInputBox_EDIT[i].IsValid && HasClicked[8]) { boxColor = RED; }
        
        if (i == ActiveBox) boxColor = SKYBLUE; // Active input
        else if (CheckCollisionPointRec(Mouse, MenusManagementInputBox_EDIT[i].Box)) boxColor = YELLOW; // Hover effect

        DrawRectangleRec(MenusManagementInputBox_EDIT[i].Box, boxColor);
        DrawTextEx(GIFont, MenusManagementInputBox_EDIT[i].Text, (Vector2){MenusManagementInputBox_EDIT[i].Box.x + 10, MenusManagementInputBox_EDIT[i].Box.y + 15}, (SchoolDataInputBoxSize.y / 2), SPACING_WIDER, BLACK);

        if (i == ActiveBox && (FrameCounter / CURSOR_BLINK_SPEED) % 2 == 0) {
            int textWidth = MeasureTextEx(GIFont, MenusManagementInputBox_EDIT[i].Text, (SchoolDataInputBoxSize.y / 2), SPACING_WIDER).x;
            DrawTextEx(GIFont, "|", (Vector2){MenusManagementInputBox_EDIT[i].Box.x + 10 + textWidth, MenusManagementInputBox_EDIT[i].Box.y + 10}, (SchoolDataInputBoxSize.y / 2) + 10, SPACING_WIDER, BLACK);
        }
        
        DrawTextEx(GIFont, MaxCharsInfo[i].TextFill, MaxCharsInfo[i].TextPosition, MaxCharsInfo[i].FontData.x, MaxCharsInfo[i].FontData.y, BLACK);
        if (strlen(MenusManagementInputBox_EDIT[i].Text) == 0 && i != ActiveBox) {
            DrawTextEx(GIFont, InputBoxHelp[i].TextFill, InputBoxHelp[i].TextPosition, InputBoxHelp[i].FontData.x, InputBoxHelp[i].FontData.y, Fade(BLACK, 0.6));
        }
    }

    DrawTextEx(GIFont, MenuInputMessage.TextFill, MenuInputMessage.TextPosition, MenuInputMessage.FontData.x, MenuInputMessage.FontData.y, WHITE);
    DrawTextEx(GIFont, EmptyMenuInputMessage.TextFill, EmptyMenuInputMessage.TextPosition, EmptyMenuInputMessage.FontData.x, EmptyMenuInputMessage.FontData.y, GOLD);
}

void VendorManageDailyMenusMenu_DELETE(void) {
    char ReturnMenuState[256] = { 0 };

    PreviousMenu = MENU_MAIN_VENDOR_MenusManagement;
    DrawPreviousMenuButton();

    GetMenuState(CurrentMenu, ReturnMenuState, false);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H2, SPACING_WIDER, (Vector2){0, 60}, 0, true);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

    const char *DataTexts[] = {
        "Apakah Anda yakin untuk MENGHAPUS data Menu M.S.G. terkait?",
        "Proses ini akan berdampak secara permanen apabila dilanjutkan, dimohon untuk berhati-hati!",

        "Menu Pokok (Karbohidrat):",
        "Menu Lauk-pauk (Protein):",
        "Menu Serat dan Mineral (Sayur-sayuran):",
        "Menu Serat dan Mineral (Buah-buahan):",
        "Sempurna (Susu):",

        "Anda telah berhasil MENGHAPUS data Menu M.S.G. terkait dari program D'Magis ini!",
        "Setelah ini Anda akan di bawa kembali ke menu `[MANAGE]: Menu Program D'Magis Vendor`...",
        "Tekan di mana saja untuk melanjutkan...",
    };
    const char *ButtonTexts[2] = {"Tidak...", "Iya..."};
    
    int ResetSubmitButtonGap = 40, ButtonsGap = 80;
    Vector2 MenuDeletionInputBoxSize = {900, 60};
    Vector2 ResetSubmitButtonSize = {(MenuDeletionInputBoxSize.x / 2) - (ButtonsGap / 4), 60};
    
    // // //
    MenusManagementInputBox_DELETE[0].Box.x = (SCREEN_WIDTH / 2) - (MenuDeletionInputBoxSize.x / 2);
    MenusManagementInputBox_DELETE[0].Box.y = 480;
    MenusManagementInputBox_DELETE[0].Box.width = MenuDeletionInputBoxSize.x;
    MenusManagementInputBox_DELETE[0].Box.height = MenuDeletionInputBoxSize.y;

    MenusManagementInputBox_DELETE[1].Box.x = MenusManagementInputBox_DELETE[0].Box.x;
    MenusManagementInputBox_DELETE[1].Box.y = MenusManagementInputBox_DELETE[0].Box.y + ButtonsGap;
    MenusManagementInputBox_DELETE[1].Box.width = MenuDeletionInputBoxSize.x;
    MenusManagementInputBox_DELETE[1].Box.height = MenuDeletionInputBoxSize.y;

    MenusManagementInputBox_DELETE[2].Box.x = MenusManagementInputBox_DELETE[1].Box.x;
    MenusManagementInputBox_DELETE[2].Box.y = MenusManagementInputBox_DELETE[1].Box.y + ButtonsGap;
    MenusManagementInputBox_DELETE[2].Box.width = MenuDeletionInputBoxSize.x;
    MenusManagementInputBox_DELETE[2].Box.height = MenuDeletionInputBoxSize.y;

    MenusManagementInputBox_DELETE[3].Box.x = MenusManagementInputBox_DELETE[2].Box.x;
    MenusManagementInputBox_DELETE[3].Box.y = MenusManagementInputBox_DELETE[2].Box.y + ButtonsGap;
    MenusManagementInputBox_DELETE[3].Box.width = MenuDeletionInputBoxSize.x;
    MenusManagementInputBox_DELETE[3].Box.height = MenuDeletionInputBoxSize.y;

    MenusManagementInputBox_DELETE[4].Box.x = MenusManagementInputBox_DELETE[3].Box.x;
    MenusManagementInputBox_DELETE[4].Box.y = MenusManagementInputBox_DELETE[3].Box.y + ButtonsGap;
    MenusManagementInputBox_DELETE[4].Box.width = MenuDeletionInputBoxSize.x;
    MenusManagementInputBox_DELETE[4].Box.height = MenuDeletionInputBoxSize.y;
    // // //

    Rectangle ResetButton = {(SCREEN_WIDTH / 2) - ResetSubmitButtonSize.x - (ResetSubmitButtonGap / 2), MenusManagementInputBox_DELETE[4].Box.y - (ButtonsGap / 4), ResetSubmitButtonSize.x, ResetSubmitButtonSize.y};
    Rectangle SubmitButton = {ResetButton.x + ResetSubmitButtonSize.x + ResetSubmitButtonGap, ResetButton.y, ResetSubmitButtonSize.x, ResetSubmitButtonSize.y};
    Rectangle Buttons[2] = {ResetButton, SubmitButton};
    Color ButtonColor[2] = { 0 }, TextColor[2] = { 0 };

    TextObject ConfirmMenuDeletionMessage[2] = {
        CreateTextObject(GIFont, DataTexts[0], FONT_H2, SPACING_WIDER, (Vector2){0, 240}, 30, true),
        CreateTextObject(GIFont, DataTexts[1], FONT_H3, SPACING_WIDER, ConfirmMenuDeletionMessage[0].TextPosition, 45, true)
    };

    TextObject DataConfirmationInfo[10] = {
        CreateTextObject(GIFont, DataTexts[2], FONT_P, SPACING_WIDER, (Vector2){MenusManagementInputBox_DELETE[0].Box.x, MenusManagementInputBox_DELETE[0].Box.y}, 0, false),
        CreateTextObject(GIFont, DataTexts[3], FONT_P, SPACING_WIDER, (Vector2){MenusManagementInputBox_DELETE[1].Box.x, MenusManagementInputBox_DELETE[1].Box.y}, -40, false),
        CreateTextObject(GIFont, DataTexts[4], FONT_P, SPACING_WIDER, (Vector2){MenusManagementInputBox_DELETE[2].Box.x, MenusManagementInputBox_DELETE[2].Box.y}, -80, false),
        CreateTextObject(GIFont, DataTexts[5], FONT_P, SPACING_WIDER, (Vector2){MenusManagementInputBox_DELETE[3].Box.x, MenusManagementInputBox_DELETE[3].Box.y}, -120, false),
        CreateTextObject(GIFont, DataTexts[6], FONT_P, SPACING_WIDER, (Vector2){MenusManagementInputBox_DELETE[4].Box.x, MenusManagementInputBox_DELETE[4].Box.y}, -160, false),

        CreateTextObject(GIFont, MenusManagementInputBox_DELETE[0].Text, FONT_P, SPACING_WIDER, (Vector2){MenusManagementInputBox_DELETE[0].Box.x + MeasureTextEx(GIFont, DataTexts[4], FONT_P, SPACING_WIDER).x + 60, DataConfirmationInfo[0].TextPosition.y}, 0, false),
        CreateTextObject(GIFont, MenusManagementInputBox_DELETE[1].Text, FONT_P, SPACING_WIDER, (Vector2){MenusManagementInputBox_DELETE[1].Box.x + MeasureTextEx(GIFont, DataTexts[4], FONT_P, SPACING_WIDER).x + 60, DataConfirmationInfo[1].TextPosition.y}, 0, false),
        CreateTextObject(GIFont, MenusManagementInputBox_DELETE[2].Text, FONT_P, SPACING_WIDER, (Vector2){MenusManagementInputBox_DELETE[2].Box.x + MeasureTextEx(GIFont, DataTexts[4], FONT_P, SPACING_WIDER).x + 60, DataConfirmationInfo[2].TextPosition.y}, 0, false),
        CreateTextObject(GIFont, MenusManagementInputBox_DELETE[3].Text, FONT_P, SPACING_WIDER, (Vector2){MenusManagementInputBox_DELETE[3].Box.x + MeasureTextEx(GIFont, DataTexts[4], FONT_P, SPACING_WIDER).x + 60, DataConfirmationInfo[3].TextPosition.y}, 0, false),
        CreateTextObject(GIFont, MenusManagementInputBox_DELETE[4].Text, FONT_P, SPACING_WIDER, (Vector2){MenusManagementInputBox_DELETE[4].Box.x + MeasureTextEx(GIFont, DataTexts[4], FONT_P, SPACING_WIDER).x + 60, DataConfirmationInfo[4].TextPosition.y}, 0, false),
    };

    TextObject SuccessMessage[3] = {
        CreateTextObject(GIFont, DataTexts[7], FONT_H2, SPACING_WIDER, (Vector2){0, 520}, 0, true),
        CreateTextObject(GIFont, DataTexts[8], FONT_H3, SPACING_WIDER, SuccessMessage[0].TextPosition, 40, true),
        CreateTextObject(GIFont, DataTexts[9], FONT_H3, SPACING_WIDER, SuccessMessage[1].TextPosition, 60, true)
    };

    char PrepareMenuInputMessage[64] = { 0 }, PrepareEmptyMenuInputMessage[128] = { 0 };
    snprintf(PrepareMenuInputMessage, sizeof(PrepareMenuInputMessage), "Penghapusan Menu M.S.G. pada hari: %s.", (VendorMenuIndex == 0) ? "SENIN" : (VendorMenuIndex == 1) ? "SELASA" : (VendorMenuIndex == 2) ? "RABU" : (VendorMenuIndex == 3) ? "KAMIS" : "JUM'AT");
    TextObject MenuInputMessage = CreateTextObject(GIFont, PrepareMenuInputMessage, FONT_H5, SPACING_WIDER, (Vector2){60, 980}, 0, false);
    snprintf(PrepareEmptyMenuInputMessage, sizeof(PrepareEmptyMenuInputMessage), "Tanda \"-\" (tanpa tanda petik) mengindikasikan kategori menu tersebut adalah KOSONG.");
    TextObject EmptyMenuInputMessage = CreateTextObject(GIFont, PrepareEmptyMenuInputMessage, FONT_H5, SPACING_WIDER, (Vector2){(SCREEN_WIDTH - 60) - MeasureTextEx(GIFont, PrepareEmptyMenuInputMessage, FONT_H5, SPACING_WIDER).x, 980}, 0, false);

    DrawManageViewBorder(0);

    Vector2 Mouse = GetMousePosition();
    if (HasClicked[9]) {
        DrawTextEx(GIFont, SuccessMessage[0].TextFill, SuccessMessage[0].TextPosition, SuccessMessage[0].FontData.x, SuccessMessage[0].FontData.y, GREEN);
        DrawTextEx(GIFont, SuccessMessage[1].TextFill, SuccessMessage[1].TextPosition, SuccessMessage[1].FontData.x, SuccessMessage[1].FontData.y, SKYBLUE);
        DrawTextEx(GIFont, SuccessMessage[2].TextFill, SuccessMessage[2].TextPosition, SuccessMessage[2].FontData.x, SuccessMessage[2].FontData.y, WHITE);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            PreviousMenu = MENU_MAIN_VENDOR_MenusManagement;
            NextMenu = MENU_MAIN_VENDOR_MenusManagement;
            CurrentMenu = NextMenu;

            PlaySound(ButtonClickSFX);
            HasClicked[9] = false;
            Transitioning = true;
            ScreenFade = 1.0f;
        }
    } else {
        for (int i = 0; i < 2; i++) {
            DrawTextEx(GIFont, ConfirmMenuDeletionMessage[i].TextFill, ConfirmMenuDeletionMessage[i].TextPosition, ConfirmMenuDeletionMessage[i].FontData.x, ConfirmMenuDeletionMessage[i].FontData.y, (i % 2 == 0) ? ORANGE : PINK);
        }

        for (int i = 0; i < 2; i++) {
            if (CheckCollisionPointRec(Mouse, Buttons[i])) {
                HoverTransition[i + 977] = CustomLerp(HoverTransition[i + 977], 1.0f, 0.1f);
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    if (i == 0) NextMenu = MENU_MAIN_VENDOR_MenusManagement;
                    if (i == 1) {
                        ReadUserAccount(Vendors);
                        OriginalVendors[User.VendorID].Menus[VendorMenuIndex].Carbohydrate[0] = '\0';
                        OriginalVendors[User.VendorID].Menus[VendorMenuIndex].Protein[0] = '\0';
                        OriginalVendors[User.VendorID].Menus[VendorMenuIndex].Vegetables[0] = '\0';
                        OriginalVendors[User.VendorID].Menus[VendorMenuIndex].Fruits[0] = '\0';
                        OriginalVendors[User.VendorID].Menus[VendorMenuIndex].Milk[0] = '\0';
                        OriginalVendors[User.VendorID].BudgetPlanConfirmation[VendorMenuIndex] = 0;
                        
                        WriteUserAccount(OriginalVendors);
                        SyncVendorsFromOriginalVendorData(Vendors, OriginalVendors);

                        HasClicked[9] = true;
                    }
                    
                    PlaySound(ButtonClickSFX);
                    Transitioning = true;
                    ScreenFade = 1.0f;
                }
            } else HoverTransition[i + 977] = CustomLerp(HoverTransition[i + 977], 0.0f, 0.1f);
            
            if (i == 0) { // Reset button
                ButtonColor[i] = (HoverTransition[i + 977] > 0.5f) ? RED : LIGHTGRAY;
                TextColor[i] = (HoverTransition[i + 977] > 0.5f) ? WHITE : BLACK;
            } else if (i == 1) { // Submit button
                ButtonColor[i] = (HoverTransition[i + 977] > 0.5f) ? GREEN : LIGHTGRAY;
                TextColor[i] = (HoverTransition[i + 977] > 0.5f) ? WHITE : BLACK;
            }

            float scale = 1.0f + HoverTransition[i + 977] * 0.1f;
            
            float newWidth = Buttons[i].width * scale;
            float newHeight = Buttons[i].height * scale;
            float newX = Buttons[i].x - (newWidth - Buttons[i].width) / 2;
            float newY = Buttons[i].y - (newHeight - Buttons[i].height) / 2;
            
            DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, ButtonColor[i]);
            
            Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[i], (FONT_P * scale), SPACING_WIDER);
            int textX = newX + (newWidth / 2) - (textWidth.x / 2);
            int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
            DrawTextEx(GIFont, ButtonTexts[i], (Vector2){textX, textY}, (FONT_P * scale), SPACING_WIDER, TextColor[i]);
        }

        DrawTextEx(GIFont, DataConfirmationInfo[0].TextFill, DataConfirmationInfo[0].TextPosition, DataConfirmationInfo[0].FontData.x, DataConfirmationInfo[0].FontData.y, LIGHTGRAY);
        DrawTextEx(GIFont, DataConfirmationInfo[5].TextFill, DataConfirmationInfo[5].TextPosition, DataConfirmationInfo[5].FontData.x, DataConfirmationInfo[5].FontData.y, WHITE);
        
        DrawTextEx(GIFont, DataConfirmationInfo[1].TextFill, DataConfirmationInfo[1].TextPosition, DataConfirmationInfo[1].FontData.x, DataConfirmationInfo[1].FontData.y, LIGHTGRAY);
        DrawTextEx(GIFont, DataConfirmationInfo[6].TextFill, DataConfirmationInfo[6].TextPosition, DataConfirmationInfo[6].FontData.x, DataConfirmationInfo[6].FontData.y, WHITE);
        
        DrawTextEx(GIFont, DataConfirmationInfo[2].TextFill, DataConfirmationInfo[2].TextPosition, DataConfirmationInfo[2].FontData.x, DataConfirmationInfo[2].FontData.y, LIGHTGRAY);
        DrawTextEx(GIFont, DataConfirmationInfo[7].TextFill, DataConfirmationInfo[7].TextPosition, DataConfirmationInfo[7].FontData.x, DataConfirmationInfo[7].FontData.y, WHITE);

        DrawTextEx(GIFont, DataConfirmationInfo[3].TextFill, DataConfirmationInfo[3].TextPosition, DataConfirmationInfo[3].FontData.x, DataConfirmationInfo[3].FontData.y, LIGHTGRAY);
        DrawTextEx(GIFont, DataConfirmationInfo[8].TextFill, DataConfirmationInfo[8].TextPosition, DataConfirmationInfo[8].FontData.x, DataConfirmationInfo[8].FontData.y, WHITE);
        
        DrawTextEx(GIFont, DataConfirmationInfo[4].TextFill, DataConfirmationInfo[4].TextPosition, DataConfirmationInfo[4].FontData.x, DataConfirmationInfo[4].FontData.y, LIGHTGRAY);
        DrawTextEx(GIFont, DataConfirmationInfo[9].TextFill, DataConfirmationInfo[9].TextPosition, DataConfirmationInfo[9].FontData.x, DataConfirmationInfo[9].FontData.y, WHITE);
    }

    DrawTextEx(GIFont, MenuInputMessage.TextFill, MenuInputMessage.TextPosition, MenuInputMessage.FontData.x, MenuInputMessage.FontData.y, WHITE);
    DrawTextEx(GIFont, EmptyMenuInputMessage.TextFill, EmptyMenuInputMessage.TextPosition, EmptyMenuInputMessage.FontData.x, EmptyMenuInputMessage.FontData.y, GOLD);
}

void VendorGetAffiliatedSchoolData(void) {
    char ReturnMenuState[256] = { 0 };

    PreviousMenu = MENU_MAIN_VENDOR;
    DrawPreviousMenuButton();

    GetMenuState(CurrentMenu, ReturnMenuState, false);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H2, SPACING_WIDER, (Vector2){0, 60}, 0, true);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

    const char *MenuDays[SCHOOL_DAYS + 2] = {"Menu M.S.G. di hari: MINGGU (libur)", "Menu M.S.G. di hari: SENIN", "Menu M.S.G. di hari: SELASA", "Menu M.S.G. di hari: RABU", "Menu M.S.G. di hari: KAMIS", "Menu M.S.G. di hari: JUM'AT", "Menu M.S.G. di hari: SABTU (libur)"};
    const char *MenuType = "Jenis Menu Makan: 4 SEHAT 5 SEMPURNA";
    const char *EachMenuPlaceholder[5] = {"Menu Pokok (Karbohidrat):", "Menu Lauk-pauk (Protein):", "Menu Serat dan Mineral (Sayur-sayuran):", "Menu Serat dan Mineral (Buah-buahan):", "Sempurna (Susu):"};
    const char *MenuStatusHelpText = "Berikut adalah Menu M.S.G. Anda sesuai HARI (realita) dengan afiliasi Sekolah terkait...";

    TextObject MenuStatusHelp = CreateTextObject(GIFont, MenuStatusHelpText, FONT_H2, SPACING_WIDER, (Vector2){0, 180}, 0, true);
    DrawTextEx(GIFont, MenuStatusHelp.TextFill, MenuStatusHelp.TextPosition, MenuStatusHelp.FontData.x, MenuStatusHelp.FontData.y, PINK);

    TextObject TO_MenuDays[SCHOOL_DAYS + 2] = {
        CreateTextObject(GIFont, MenuDays[0], FONT_H1, SPACING_WIDER, (Vector2){0, 270}, 0, true),
        CreateTextObject(GIFont, MenuDays[1], FONT_H1, SPACING_WIDER, (Vector2){0, 270}, 0, true),
        CreateTextObject(GIFont, MenuDays[2], FONT_H1, SPACING_WIDER, (Vector2){0, 270}, 0, true),
        CreateTextObject(GIFont, MenuDays[3], FONT_H1, SPACING_WIDER, (Vector2){0, 270}, 0, true),
        CreateTextObject(GIFont, MenuDays[4], FONT_H1, SPACING_WIDER, (Vector2){0, 270}, 0, true),
        CreateTextObject(GIFont, MenuDays[5], FONT_H1, SPACING_WIDER, (Vector2){0, 270}, 0, true),
        CreateTextObject(GIFont, MenuDays[6], FONT_H1, SPACING_WIDER, (Vector2){0, 270}, 0, true),
    };
    TextObject TO_MenuType = CreateTextObject(GIFont, MenuType, FONT_H2, SPACING_WIDER, (Vector2){0, TO_MenuDays[0].TextPosition.y + MeasureTextEx(GIFont, MenuType, FONT_H2, SPACING_WIDER).y}, 20, true);
    TextObject TO_EachMenuPlaceholder[5] = {
        CreateTextObject(GIFont, EachMenuPlaceholder[0], FONT_H3, SPACING_WIDER, (Vector2){180, 450}, 0, false),
        CreateTextObject(GIFont, EachMenuPlaceholder[1], FONT_H3, SPACING_WIDER, (Vector2){180, TO_EachMenuPlaceholder[0].TextPosition.y}, 45, false),
        CreateTextObject(GIFont, EachMenuPlaceholder[2], FONT_H3, SPACING_WIDER, (Vector2){180, TO_EachMenuPlaceholder[1].TextPosition.y}, 45, false),
        CreateTextObject(GIFont, EachMenuPlaceholder[3], FONT_H3, SPACING_WIDER, (Vector2){180, TO_EachMenuPlaceholder[2].TextPosition.y}, 45, false),
        CreateTextObject(GIFont, EachMenuPlaceholder[4], FONT_H3, SPACING_WIDER, (Vector2){180, TO_EachMenuPlaceholder[3].TextPosition.y}, 45, false),
    };

    TextObject TO_MenuDetails[5] = {
        CreateTextObject(GIFont, Vendors[User.VendorID].Menus[GetDaysOfWeek() - 1].Carbohydrate, FONT_H3, SPACING_WIDER, (Vector2){TO_EachMenuPlaceholder[0].TextPosition.x + MeasureTextEx(GIFont, EachMenuPlaceholder[2], FONT_H3, SPACING_WIDER).x + 60, 450}, 0, false),
        CreateTextObject(GIFont, Vendors[User.VendorID].Menus[GetDaysOfWeek() - 1].Protein, FONT_H3, SPACING_WIDER, (Vector2){TO_EachMenuPlaceholder[0].TextPosition.x + MeasureTextEx(GIFont, EachMenuPlaceholder[2], FONT_H3, SPACING_WIDER).x + 60, TO_EachMenuPlaceholder[0].TextPosition.y}, 45, false),
        CreateTextObject(GIFont, Vendors[User.VendorID].Menus[GetDaysOfWeek() - 1].Vegetables, FONT_H3, SPACING_WIDER, (Vector2){TO_EachMenuPlaceholder[0].TextPosition.x + MeasureTextEx(GIFont, EachMenuPlaceholder[2], FONT_H3, SPACING_WIDER).x + 60, TO_EachMenuPlaceholder[1].TextPosition.y}, 45, false),
        CreateTextObject(GIFont, Vendors[User.VendorID].Menus[GetDaysOfWeek() - 1].Fruits, FONT_H3, SPACING_WIDER, (Vector2){TO_EachMenuPlaceholder[0].TextPosition.x + MeasureTextEx(GIFont, EachMenuPlaceholder[2], FONT_H3, SPACING_WIDER).x + 60, TO_EachMenuPlaceholder[2].TextPosition.y}, 45, false),
        CreateTextObject(GIFont, Vendors[User.VendorID].Menus[GetDaysOfWeek() - 1].Milk, FONT_H3, SPACING_WIDER, (Vector2){TO_EachMenuPlaceholder[0].TextPosition.x + MeasureTextEx(GIFont, EachMenuPlaceholder[2], FONT_H3, SPACING_WIDER).x + 60, TO_EachMenuPlaceholder[3].TextPosition.y}, 45, false),
    };
    if (GetDaysOfWeek() == 0 || GetDaysOfWeek() == 6) {
        TO_MenuDetails[0].TextFill = "-";
        TO_MenuDetails[1].TextFill = "-";
        TO_MenuDetails[2].TextFill = "-";
        TO_MenuDetails[3].TextFill = "-";
        TO_MenuDetails[4].TextFill = "-";
    } for (int i = 0; i < 5; i++) {
        DrawTextEx(GIFont, TO_MenuDetails[i].TextFill, TO_MenuDetails[i].TextPosition, TO_MenuDetails[i].FontData.x, TO_MenuDetails[i].FontData.y, (GetDaysOfWeek() == 1) ? ColorBrightness(RED, 0.4f) : (GetDaysOfWeek() == 2) ? ColorBrightness(YELLOW, 0.4f) : (GetDaysOfWeek() == 3) ? ColorBrightness(GREEN, 0.4f) : (GetDaysOfWeek() == 4) ? ColorBrightness(SKYBLUE, 0.4f) : (GetDaysOfWeek() == 5) ? ColorBrightness(PURPLE, 0.4f) : LIGHTGRAY);
    }

    TextObject AffiliatedSchoolData[4] = {
        CreateTextObject(GIFont, "Nama Sekolah yang ter-afiliasi dengan Anda adalah:", FONT_H4, SPACING_WIDER, (Vector2){0, TO_MenuDetails[4].TextPosition.y + 105}, 0, true),
        CreateTextObject(GIFont, (strlen(Vendors[User.VendorID].AffiliatedSchoolData.Name) < 1) ? "-" : Vendors[User.VendorID].AffiliatedSchoolData.Name, FONT_H1, SPACING_WIDER, AffiliatedSchoolData[0].TextPosition, 30, true),
        CreateTextObject(GIFont, "Banyak murid yang dimiliki Sekolah terkait adalah:", FONT_H4, SPACING_WIDER, AffiliatedSchoolData[1].TextPosition, 90, true),
        CreateTextObject(GIFont, (strlen(Vendors[User.VendorID].AffiliatedSchoolData.Students) < 1) ? "..." : Vendors[User.VendorID].AffiliatedSchoolData.Students, FONT_H1, SPACING_WIDER, AffiliatedSchoolData[2].TextPosition, 30, true)
    };
    for (int i = 0; i < 4; i++) {
        DrawTextEx(GIFont, AffiliatedSchoolData[i].TextFill, AffiliatedSchoolData[i].TextPosition, AffiliatedSchoolData[i].FontData.x, AffiliatedSchoolData[i].FontData.y, (i == 1) ? YELLOW : (i == 3) ? BEIGE : WHITE);
    }

    DrawManageViewBorder(0);
    DrawTextEx(GIFont, TO_MenuDays[GetDaysOfWeek()].TextFill, TO_MenuDays[GetDaysOfWeek()].TextPosition, TO_MenuDays[GetDaysOfWeek()].FontData.x, TO_MenuDays[GetDaysOfWeek()].FontData.y, (GetDaysOfWeek() == 1) ? ColorBrightness(RED, 0.4f) : (GetDaysOfWeek() == 2) ? ColorBrightness(YELLOW, 0.4f) : (GetDaysOfWeek() == 3) ? ColorBrightness(GREEN, 0.4f) : (GetDaysOfWeek() == 4) ? ColorBrightness(SKYBLUE, 0.4f) : (GetDaysOfWeek() == 5) ? ColorBrightness(PURPLE, 0.4f) : LIGHTGRAY);
    DrawTextEx(GIFont, TO_MenuType.TextFill, TO_MenuType.TextPosition, TO_MenuType.FontData.x, TO_MenuType.FontData.y, ORANGE);
    for (int i = 0; i < 5; i++) {
        DrawTextEx(GIFont, TO_EachMenuPlaceholder[i].TextFill, TO_EachMenuPlaceholder[i].TextPosition, TO_EachMenuPlaceholder[i].FontData.x, TO_EachMenuPlaceholder[i].FontData.y, (GetDaysOfWeek() == 0 || GetDaysOfWeek() == 6) ? GRAY : LIGHTGRAY);
    }

    DrawLineEx((Vector2){120, TO_MenuType.TextPosition.y + 75}, (Vector2){(SCREEN_WIDTH - 120), TO_MenuType.TextPosition.y + 75}, 2.0f, WHITE);
    DrawLineEx((Vector2){120, TO_MenuDetails[4].TextPosition.y + 75}, (Vector2){(SCREEN_WIDTH - 120), TO_MenuDetails[4].TextPosition.y + 75}, 2.0f, WHITE);
}

void VendorRequestBudgetPlanMenu(void) {
    char ReturnMenuState[256] = { 0 };

    PreviousMenu = MENU_MAIN_VENDOR;
    DrawPreviousMenuButton();

    GetMenuState(CurrentMenu, ReturnMenuState, false);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H2, SPACING_WIDER, (Vector2){0, 60}, 0, true);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

    Vector2 Mouse = GetMousePosition();

    if (!IsCardTransitioning) {
        // if (CheckCollisionPointRec(mouse, nextButton) && currentProfile < MAX_PROFILES - 1) {
        if (IsKeyPressed(KEY_RIGHT) && CurrentCardMenuProfile < (SCHOOL_DAYS - 1)) {
            TransitionDirection = 1; // Move right
            IsCardTransitioning = true;
            TargetX = SCREEN_WIDTH; // Move to the right
        }

        // if (CheckCollisionPointRec(mouse, prevButton) && currentProfile > 0) {
        if (IsKeyPressed(KEY_LEFT) && CurrentCardMenuProfile > 0) {
            TransitionDirection = -1; // Move left
            IsCardTransitioning = true;
            TargetX = -SCREEN_WIDTH; // Move to the left
        }
    }

    Rectangle NavigationButtons[2] = {
        {75, (SCREEN_HEIGHT / 2) - (210 / 2), 90, 210},
        {(SCREEN_WIDTH - 75) - 90, (SCREEN_HEIGHT / 2) - (210 / 2), 90, 210},
    };
    Color NavigationButtonColor[2], NavigationTextColor[2];
    const char *NavigationButtonsTexts[] = {"[<]", "[>]"};

    for (int i = 801; i < 803; i++) {
        if (CheckCollisionPointRec(Mouse, NavigationButtons[i - 801])) {
            HoverTransition[i] = CustomLerp(HoverTransition[i], 1.0f, 0.1f);
            
            if (!IsCardTransitioning) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    PlaySound(ButtonClickSFX);

                    if (i == 802 && CurrentCardMenuProfile < (SCHOOL_DAYS - 1)) {
                        TransitionDirection = 1; // Move right
                        IsCardTransitioning = true;
                        TargetX = SCREEN_WIDTH; // Move to the right
                    }

                    if (i == 801 && CurrentCardMenuProfile > 0) {
                        TransitionDirection = -1; // Move left
                        IsCardTransitioning = true;
                        TargetX = -SCREEN_WIDTH; // Move to the left
                    }
                }

            }

        } else HoverTransition[i] = CustomLerp(HoverTransition[i], 0.0f, 0.1f);
        
        if (i == 801) {
            NavigationButtonColor[i - 801] = (HoverTransition[i] > 0.5f) ? PURPLE : GRAY;
            NavigationTextColor[i - 801] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        } else if (i == 802) {
            NavigationButtonColor[i - 801] = (HoverTransition[i] > 0.5f) ? SKYBLUE : GRAY;
            NavigationTextColor[i - 801] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
        }

        float scale = 1.0f + HoverTransition[i] * 0.1f;
        
        float newWidth = NavigationButtons[i - 801].width * scale;
        float newHeight = NavigationButtons[i - 801].height * scale;
        float newX = NavigationButtons[i - 801].x - (newWidth - NavigationButtons[i - 801].width) / 2;
        float newY = NavigationButtons[i - 801].y - (newHeight - NavigationButtons[i - 801].height) / 2;
        
        DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, NavigationButtonColor[i - 801]);
        
        Vector2 textWidth = MeasureTextEx(GIFont, NavigationButtonsTexts[(i - 801)], (FONT_H1 * scale), SPACING_WIDER);
        int textX = newX + (newWidth / 2) - (textWidth.x / 2);
        int textY = newY + (newHeight / 2) - (FONT_H1 * scale / 2);
        DrawTextEx(GIFont, NavigationButtonsTexts[(i - 801)], (Vector2){textX, textY}, (FONT_H1 * scale), SPACING_WIDER, NavigationTextColor[i - 801]);
    }

    // LERP Animation for Smooth Transition
    if (IsCardTransitioning) {
        SetCardPositionInX = CustomLerp(SetCardPositionInX, TargetX, 0.1f);

        // Stop transition when close enough
        if (fabs(SetCardPositionInX - TargetX) < 1.0f) {
            IsCardTransitioning = false;
            CurrentCardMenuProfile += TransitionDirection;
            SetCardPositionInX = 0; // Reset position
            TargetX = 0;
        }
    }

    // Draw IsCardTransitioning Profiles
    DrawMenuProfileCard(User.VendorID, CurrentCardMenuProfile, SetCardPositionInX, true, false, false);
    if (IsCardTransitioning) {
        DrawMenuProfileCard(User.VendorID, CurrentCardMenuProfile + TransitionDirection, SetCardPositionInX + (-TransitionDirection * SCREEN_WIDTH), true, false, false);
    }
}

void VendorGetDailyMenuStatusList(void) {
    char ReturnMenuState[256] = { 0 };
    int LeftMargin = 40, TopMargin = 20;

    PreviousMenu = MENU_MAIN_VENDOR;
    DrawPreviousMenuButton();

    GetMenuState(CurrentMenu, ReturnMenuState, false);
    TextObject SubtitleMessage = CreateTextObject(GIFont, ReturnMenuState, FONT_H2, SPACING_WIDER, (Vector2){0, 60}, 0, true);
    DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

    const char *MenuDays[SCHOOL_DAYS] = {"Menu M.S.G. di hari: SENIN", "Menu M.S.G. di hari: SELASA", "Menu M.S.G. di hari: RABU", "Menu M.S.G. di hari: KAMIS", "Menu M.S.G. di hari: JUM'AT"};
    const char *MenuStatusHelpText = "Anda dapat melihat data ringkas dari status pengajuan menu-menu Anda di sini...";
    const char *StatusExplanations[4] = {
        "[-]: BELUM DIAJUKAN (warna abu-abu)",
        "[?]: SEDANG DITINJAU (warna kuning)",
        "[OK]: PENGAJUAN DITERIMA (warna hijau)",
        "[!]: PENGAJUAN DITOLAK (warna merah)"
    };
    
    TextObject MenuStatusHelp = CreateTextObject(GIFont, MenuStatusHelpText, FONT_H2, SPACING_WIDER, (Vector2){0, 180}, 0, true);
    DrawTextEx(GIFont, MenuStatusHelp.TextFill, MenuStatusHelp.TextPosition, MenuStatusHelp.FontData.x, MenuStatusHelp.FontData.y, PINK);

    TextObject DisplayStatusExplanations[4] = {
        CreateTextObject(GIFont, StatusExplanations[0], FONT_H5, SPACING_WIDER, (Vector2){60, 980}, 0, false),
        CreateTextObject(GIFont, StatusExplanations[1], FONT_H5, SPACING_WIDER, DisplayStatusExplanations[0].TextPosition, 30, false),
        CreateTextObject(GIFont, StatusExplanations[2], FONT_H5, SPACING_WIDER, (Vector2){(SCREEN_WIDTH - 60 - MeasureTextEx(GIFont, StatusExplanations[2], FONT_H5, SPACING_WIDER).x), 980}, 0, false),
        CreateTextObject(GIFont, StatusExplanations[3], FONT_H5, SPACING_WIDER, (Vector2){(SCREEN_WIDTH - 60 - MeasureTextEx(GIFont, StatusExplanations[3], FONT_H5, SPACING_WIDER).x), 980}, 30, false)
    };
    for (int i = 0; i < 4; i++) {
        DrawTextEx(GIFont, DisplayStatusExplanations[i].TextFill, DisplayStatusExplanations[i].TextPosition, DisplayStatusExplanations[i].FontData.x, DisplayStatusExplanations[i].FontData.y, (i == 0) ? GRAY : (i == 1) ? YELLOW : (i == 2) ? GREEN : (i == 3) ? RED : WHITE);
    }

    char VAD_No[4] = { 0 }, VAD_GeneratedSecondLineMeasurement[32] = { 0 };
    TextObject VAD_Numberings[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject VAD_FirstLine[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject VAD_SchoolName[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject VAD_SecondLine[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject VAD_VendorName[MAX_ACCOUNT_LIMIT] = { 0 };
    TextObject VAD_ThirdLine[MAX_ACCOUNT_LIMIT] = { 0 };
    Color SetBPFontColor[SCHOOL_DAYS];

    DrawManageViewBorder(SCHOOL_DAYS);

    for (int i = 0; i < SCHOOL_DAYS; i++) {
        snprintf(VAD_No, sizeof(VAD_No), "%01d", (i + 1));
        VAD_Numberings[i].TextFill = VAD_No;
        VAD_Numberings[i].TextPosition = (Vector2){60 + LeftMargin, 240 + (144 * i) + ((TopMargin * 5) / 2) + 3};
        VAD_Numberings[i].FontData = (Vector2){FONT_H2, SPACING_WIDER};
        DrawTextEx(GIFont, VAD_Numberings[i].TextFill, VAD_Numberings[i].TextPosition, VAD_Numberings[i].FontData.x, VAD_Numberings[i].FontData.y, SKYBLUE);

        VAD_FirstLine[i].TextFill = "Kategori Menu Harian:";
        VAD_FirstLine[i].TextPosition = (Vector2){
            (60 + LeftMargin) + (MeasureTextEx(GIFont, "0", VAD_Numberings[i].FontData.x, VAD_Numberings[i].FontData.y).x + LeftMargin),
            240 + (144 * i) + (TopMargin / 2)
        };
        VAD_FirstLine[i].FontData = (Vector2){FONT_H5, SPACING_WIDER};
        DrawTextEx(GIFont, VAD_FirstLine[i].TextFill, VAD_FirstLine[i].TextPosition, VAD_FirstLine[i].FontData.x, VAD_FirstLine[i].FontData.y, WHITE);

        VAD_VendorName[i].TextFill = MenuDays[i];
        VAD_VendorName[i].TextPosition = (Vector2){
            VAD_FirstLine[i].TextPosition.x,
            VAD_FirstLine[i].TextPosition.y + MeasureTextEx(GIFont, VAD_Numberings[i].TextFill, VAD_Numberings[i].FontData.x, VAD_Numberings[i].FontData.y).y - (TopMargin / 3)
        };
        VAD_VendorName[i].FontData = (Vector2){(strlen(VAD_VendorName[i].TextFill) <= 20) ? FONT_H1 : FONT_H2, SPACING_WIDER};
        VAD_VendorName[i].TextPosition.y += (VAD_VendorName[i].FontData.x == FONT_H1) ? 0 : 7;
        DrawTextEx(GIFont, VAD_VendorName[i].TextFill, VAD_VendorName[i].TextPosition, VAD_VendorName[i].FontData.x, VAD_VendorName[i].FontData.y, PURPLE);

        for (int c = 0; c < MAX_INPUT_LENGTH; c++) { VAD_GeneratedSecondLineMeasurement[c] = 'O'; }
        VAD_SecondLine[i].TextFill = "Status Pengajuan Menu:";
        VAD_SecondLine[i].TextPosition = (Vector2){
            (((60 + LeftMargin) + VAD_FirstLine[i].TextPosition.x) * 2) + MeasureTextEx(GIFont, VAD_GeneratedSecondLineMeasurement, VAD_FirstLine[i].FontData.x, VAD_FirstLine[i].FontData.y).x,
            240 + (144 * i) + (TopMargin / 2)
        };
        VAD_SecondLine[i].FontData = (Vector2){FONT_H5, SPACING_WIDER};
        DrawTextEx(GIFont, VAD_SecondLine[i].TextFill, VAD_SecondLine[i].TextPosition, VAD_SecondLine[i].FontData.x, VAD_SecondLine[i].FontData.y, WHITE);
        VAD_GeneratedSecondLineMeasurement[0] = '\0';
        
        if (Vendors[User.VendorID].BudgetPlanConfirmation[i] == 2)          { VAD_SchoolName[i].TextFill = "[OK] PENGAJUAN DITERIMA"; SetBPFontColor[i] = GREEN; }
        else if (Vendors[User.VendorID].BudgetPlanConfirmation[i] == 1)     { VAD_SchoolName[i].TextFill = "[?] SEDANG DITINJAU"; SetBPFontColor[i] = YELLOW; }
        else if (Vendors[User.VendorID].BudgetPlanConfirmation[i] == 0)     { VAD_SchoolName[i].TextFill = "[-] BELUM DIAJUKAN"; SetBPFontColor[i] = GRAY; }
        else if (Vendors[User.VendorID].BudgetPlanConfirmation[i] == -1)    { VAD_SchoolName[i].TextFill = "[!] PENGAJUAN DITOLAK"; SetBPFontColor[i] = RED; }
        VAD_SchoolName[i].TextPosition = (Vector2){
            VAD_SecondLine[i].TextPosition.x,
            VAD_SecondLine[i].TextPosition.y + MeasureTextEx(GIFont, VAD_SecondLine[i].TextFill, VAD_SecondLine[i].FontData.x, VAD_SecondLine[i].FontData.y).y + (TopMargin + 5)
        };
        VAD_SchoolName[i].FontData = (Vector2){(strlen(VAD_SchoolName[i].TextFill) <= 20) ? FONT_H1 : FONT_H2, SPACING_WIDER};
        VAD_SchoolName[i].TextPosition.y -= (VAD_SchoolName[i].FontData.x == FONT_H1) ? 12 : 6;
        DrawTextEx(GIFont, VAD_SchoolName[i].TextFill, VAD_SchoolName[i].TextPosition, VAD_SchoolName[i].FontData.x, VAD_SchoolName[i].FontData.y, SetBPFontColor[i]);

        if (Vendors[User.VendorID].BudgetPlanConfirmation[i] == 2)          { VAD_ThirdLine[i].TextFill = "Menu ini TIDAK DAPAT diubah kembali karena telah disetujui oleh Pemerintah. Hapuslah menu ini terlebih dahulu apabila ingin melakukan perubahan."; }
        else if (Vendors[User.VendorID].BudgetPlanConfirmation[i] == 1)     { VAD_ThirdLine[i].TextFill = "Menu ini MASIH DALAM proses peninjauan dari Pemerintah. Anda masih dapat melakukan perubahan/menghapus menu ini selagi masih diajukan."; }
        else if (Vendors[User.VendorID].BudgetPlanConfirmation[i] == 0)     { VAD_ThirdLine[i].TextFill = "Menu ini BELUM diajukan dikarenakan belum/baru ditambahkannya menu ini. Silakan untuk diajukan apabila dirasa sudah siap pada menu: \"[REQUEST]: Pengajuan RAB\"."; }
        else if (Vendors[User.VendorID].BudgetPlanConfirmation[i] == -1)    { VAD_ThirdLine[i].TextFill = "Menu ini TELAH DITOLAK oleh Pemerintah. Selagi masih belum diterima, Anda dapat mengajukan perubahan menu ini yang terbaru setelahnya."; }
        VAD_ThirdLine[i].TextPosition = (Vector2){
            VAD_FirstLine[i].TextPosition.x,
            (278 + (144 * i) + TopMargin) + MeasureTextEx(GIFont, VAD_SchoolName[i].TextFill, VAD_SchoolName[i].FontData.x, VAD_SchoolName[i].FontData.y).y + (VAD_SchoolName[i].FontData.x == FONT_H1 ? 0 : 12)
        };
        VAD_ThirdLine[i].FontData = (Vector2){FONT_H6, SPACING_WIDER};
        DrawTextEx(GIFont, VAD_ThirdLine[i].TextFill, VAD_ThirdLine[i].TextPosition, VAD_ThirdLine[i].FontData.x, VAD_ThirdLine[i].FontData.y, SetBPFontColor[i]);
    }
}

// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------

int main(void) {

#if 0
    memset(OriginalVendors, 0, sizeof(VendorDatabase) * MAX_ACCOUNT_LIMIT);
    memset(OriginalSchools, 0, sizeof(SchoolDatabase) * MAX_ACCOUNT_LIMIT);
    WriteUserAccount(OriginalVendors);
    WriteSchoolsData(OriginalSchools);

    // for (int i = 0; i < MAX_ACCOUNT_LIMIT; i++) {
    //     snprintf(Vendors[i].Username, sizeof(Vendors[i].Username), "Catering ID.: %03d", (i + 1));
    //     snprintf(Vendors[i].Password, sizeof(Vendors[i].Password), "vendor%03d", (i + 1));
    //     strcpy(Vendors[i].AffiliatedSchoolData.Name, "");
    //     strcpy(Vendors[i].AffiliatedSchoolData.Students, "");
    //     strcpy(Vendors[i].AffiliatedSchoolData.AffiliatedVendor, "");
    //     for (int j = 0; j < (5 - 2); j++) {
    //         snprintf(Vendors[i].Menus[j].Carbohydrate, sizeof(Vendors[i].Menus[j].Carbohydrate), "Some Carbohydrate %03d", (i + 1));
    //         snprintf(Vendors[i].Menus[j].Protein, sizeof(Vendors[i].Menus[j].Protein), "Some Protein %03d", (i + 1));
    //         snprintf(Vendors[i].Menus[j].Vegetables, sizeof(Vendors[i].Menus[j].Vegetables), "Some Vegetables %03d", (i + 1));
    //         snprintf(Vendors[i].Menus[j].Fruits, sizeof(Vendors[i].Menus[j].Fruits), "Some Fruits %03d", (i + 1));
    //         snprintf(Vendors[i].Menus[j].Milk, sizeof(Vendors[i].Menus[j].Milk), "Some Milk %03d", (i + 1));
    //         Vendors[i].BudgetPlanConfirmation[j] = 1;
    //     }
    //     Vendors[i].SaveSchoolDataIndex = -1;
    //     WriteUserAccount(Vendors);
    // }

    // for (int i = 0; i < MAX_ACCOUNT_LIMIT; i++) {
    //     snprintf(Schools[i].Name, sizeof(Schools[i].Name), "School No.: %03d", (i + 1));
    //     snprintf(Schools[i].Students, sizeof(Schools[i].Students), "%03d", GetRandomValue(10, 9999));
    //     strcpy(Schools[i].AffiliatedVendor, "");
    //     WriteSchoolsData(Schools);
    // }

    return 0;
}
#else
    emscripten_set_canvas_element_size("#canvas", 1920, 1080);
    emscripten_set_timeout(SyncFileSystemInit, 500, NULL);  // Delay FS setup by 500ms

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    SetTraceLogLevel(LOG_ALL);

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "D'Magis - Program Makan Siang Gratis");
    SetTargetFPS(60);
// ----------------------------------------------------------------------------------------------------
    GLuint DefaultVAO;
    glGenVertexArrays(1, &DefaultVAO);
    glBindVertexArray(DefaultVAO);

    static Triangle triangles[MAX_TRIANGLES] = { 0 };
    for (int i = 0; i < MAX_TRIANGLES; i++) {
        triangles[i].position = (Vector2){
            GetRandomValue(-400, 400),
            GetRandomValue(SCREEN_HEIGHT, SCREEN_HEIGHT + 400)
        };
        triangles[i].size = GetRandomValue(30, 60);
        triangles[i].speed = GetRandomValue(10, 30) / 10.0f; // 1.0 to 3.0
        triangles[i].angle = GetRandomValue(0, 360);
        triangles[i].rotationSpeed = GetRandomValue(5, 20) / 10.0f;

        triangles[i].c1 = RandomColor();
        triangles[i].c2 = RandomColor();
        triangles[i].c3 = RandomColor();
    }
// ----------------------------------------------------------------------------------------------------
    GIFont = LoadFont("/assets/zh-cn.ttf");
    Shader shader = LoadShader("/assets/gradient.vs", "/assets/gradient.fs");

    InitAudioDevice();

    int CurrentBGMIndex = -1;
    bool ShouldChangeBGM = false;

    ButtonClickSFX = LoadSound("/assets/BUTTON_CLICK_SFX.mp3");
    Music BackgroundMusics[MAX_BGM] = {
        LoadMusicStream("/assets/SoS_ToT_01.mp3"),
        LoadMusicStream("/assets/HM_SV_02.mp3"),
        LoadMusicStream("/assets/SoS_ToT_03.mp3"),
        LoadMusicStream("/assets/HM_SV_04.mp3")
    };
    SetSoundVolume(ButtonClickSFX, 1.0f);
    SetMusicVolume(BackgroundMusics[0], 1.0f);
    SetMusicVolume(BackgroundMusics[1], 1.0f);
    SetMusicVolume(BackgroundMusics[2], 1.0f);
    SetMusicVolume(BackgroundMusics[3], 1.0f);
// ----------------------------------------------------------------------------------------------------
    PreviousMenu = MENU_WELCOME; CurrentMenu = MENU_WELCOME; NextMenu = MENU_WELCOME;
    // PreviousMenu = MENU_MAIN_GOVERNMENT; CurrentMenu = MENU_MAIN_GOVERNMENT; NextMenu = MENU_MAIN_GOVERNMENT;
    // PreviousMenu = MENU_MAIN_VENDOR; CurrentMenu = MENU_MAIN_VENDOR; NextMenu = MENU_MAIN_VENDOR;
    SortingStatus = SORT_NORMAL;
    
    TextObject TitleMessage = CreateTextObject(GIFont, TEXT_MENU_WELCOME_TitleMessage, FONT_H1, SPACING_WIDER, (Vector2){0, 100}, 0, true);
    TextObject SubtitleMessage = CreateTextObject(GIFont, TEXT_MENU_WELCOME_SubtitleMessage, FONT_H3, SPACING_WIDER, (Vector2){0, TitleMessage.TextPosition.y}, 60, true);

    for (int i = 0; i < 1024; i++) { HoverTransition[i] = 0; }
    ScreenFade = 1.0f;
    Transitioning = false;

    // ----------------------------------------------------------------------------------------------------
    ReadVendors = ReadUserAccount(OriginalVendors);
    ReadSchools = ReadSchoolsData(OriginalSchools);
    SyncVendorsFromOriginalVendorData(Vendors, OriginalVendors);
    SyncSchoolsFromOriginalSchoolData(Schools, OriginalSchools);

    // User.VendorID = 0;
    // User.Vendor = OriginalVendors[User.VendorID];
    // ----------------------------------------------------------------------------------------------------

    ActiveBox = -1;     // -1 means no active input box
    FrameCounter = 0;   // Used for blinking cursor effect
    FirstRun = true; CheckForSignInClick = false; OpenAccessForAdmin = false; OpenAccessForUser = false;
    bool ShowPassword = false;
    
    int ResetSubmitButtonGap = 40, ButtonsGap = 80;
    Vector2 WelcomeInputBoxSize = {900, 60};
    Vector2 ResetSubmitButtonSize = {(WelcomeInputBoxSize.x / 2) - (ButtonsGap / 4), 60};
    InputBox WelcomeInputList[6] = {
        // First 2 is for AdminSignIn, second 2 is for UserSignUp, third 2 is for UserSignIn
        
        {(Rectangle){(SCREEN_WIDTH / 2) - (WelcomeInputBoxSize.x / 2), 540, WelcomeInputBoxSize.x, WelcomeInputBoxSize.y}, "", "", "", false, true},
        {(Rectangle){(SCREEN_WIDTH / 2) - (WelcomeInputBoxSize.x / 2), WelcomeInputList[0].Box.y + ButtonsGap, WelcomeInputBoxSize.x, WelcomeInputBoxSize.y}, "", "", "", false, true},
        
        {(Rectangle){(SCREEN_WIDTH / 2) - (WelcomeInputBoxSize.x / 2), 540, WelcomeInputBoxSize.x, WelcomeInputBoxSize.y}, "", "", "", false, true},
        {(Rectangle){(SCREEN_WIDTH / 2) - (WelcomeInputBoxSize.x / 2), WelcomeInputList[2].Box.y + ButtonsGap, WelcomeInputBoxSize.x, WelcomeInputBoxSize.y}, "", "", "", false, true},

        {(Rectangle){(SCREEN_WIDTH / 2) - (WelcomeInputBoxSize.x / 2), 540, WelcomeInputBoxSize.x, WelcomeInputBoxSize.y}, "", "", "", false, true},
        {(Rectangle){(SCREEN_WIDTH / 2) - (WelcomeInputBoxSize.x / 2), WelcomeInputList[4].Box.y + ButtonsGap, WelcomeInputBoxSize.x, WelcomeInputBoxSize.y}, "", "", "", false, true},
    };

    while (!WindowShouldClose()) {
// ----------------------------------------------------------------------------------------------------
        struct tm *WholeDate;
        const char *DaysOfWeek[SCHOOL_DAYS + 2] = {"Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jum'at", "Sabtu"};
        char ShowLocalTime[32] = { 0 };

        GetWholeDate(&WholeDate);
        snprintf(ShowLocalTime, sizeof(ShowLocalTime), "%s, %02d/%02d/%04d [%02d:%02d:%02d]", DaysOfWeek[GetDaysOfWeek()], WholeDate->tm_mday, WholeDate->tm_mon, (1900 + WholeDate->tm_year), WholeDate->tm_hour, WholeDate->tm_min, WholeDate->tm_sec);
        TextObject LocalTime_GUI = CreateTextObject(GIFont, ShowLocalTime, FONT_H5, SPACING_WIDER, (Vector2){(SCREEN_WIDTH - MeasureTextEx(GIFont, ShowLocalTime, FONT_H5, SPACING_WIDER).x) - 20, 20}, 0, false);
        DrawTextEx(GIFont, LocalTime_GUI.TextFill, LocalTime_GUI.TextPosition, LocalTime_GUI.FontData.x, LocalTime_GUI.FontData.y, WHITE);
// ----------------------------------------------------------------------------------------------------
        GetLastSignInTime(&ST_Admin, "/data/"ADMIN_SAVEFILE);
        GetLastSignInTime(&ST_User, "/data/"USER_SAVEFILE);
        GetCurrentRealTime(&ST_Admin);
        GetCurrentRealTime(&ST_User);
        
        if (FirstRun) {
            if (ST_Admin.SignInChance >= 0) {
                ST_Admin.AllValid = false;
                ST_Admin.CooldownTime = 15;
                ST_Admin.CurrentTime = 0;
                ST_Admin.LastTime = 0;
                ST_Admin.RemainingTime = 0;
                ST_Admin.SignInChance = 3; // Default sign-in chances
                UpdateLastSignInTime(&ST_Admin, "/data/"ADMIN_SAVEFILE);
            }
            
            if (ST_User.SignInChance >= 0) {
                ST_User.AllValid = false;
                ST_User.CooldownTime = 15;
                ST_User.CurrentTime = 0;
                ST_User.LastTime = 0;
                ST_User.RemainingTime = 0;
                ST_User.SignInChance = 3; // Default sign-in chances
                UpdateLastSignInTime(&ST_User, "/data/"USER_SAVEFILE);
            }
            
            FirstRun = false;
        
        } else {
            ST_Admin.RemainingTime = (ST_Admin.LastTime == 0) ? 0 : ST_Admin.CooldownTime - (ST_Admin.CurrentTime - ST_Admin.LastTime);
            ST_User.RemainingTime = (ST_User.LastTime == 0) ? 0 : ST_User.CooldownTime - (ST_User.CurrentTime - ST_User.LastTime);
        }

        bool OpenAccess[2] = { (ST_Admin.RemainingTime <= 0), (ST_User.RemainingTime <= 0) };
        for (int i = 0; i < 2; i++) {
            SetTime* CurrentST = (i == 0) ? &ST_Admin : &ST_User;
            const char* FilePath = (i == 0) ? "/data/"ADMIN_SAVEFILE : "/data/"USER_SAVEFILE;

            if (OpenAccess[i] && CurrentST->SignInChance < 0) {
                CurrentST->SignInChance = 3;
                UpdateLastSignInTime(CurrentST, FilePath);
            }
        }
        
        // TraceLog(LOG_INFO, "Admin: %d sec left, Chances: %d", ST_Admin.RemainingTime, ST_Admin.SignInChance);
        // TraceLog(LOG_INFO, "User: %d sec left, Chances: %d", ST_User.RemainingTime, ST_User.SignInChance);
// ----------------------------------------------------------------------------------------------------
        Vector2 Mouse = GetMousePosition();
        int AdminUserButtonGap = 40;
        Vector2 AdminUserButtonSize = {400, 400}, AboutButtonSize = {(AdminUserButtonSize.x * 2) + AdminUserButtonGap, 60}, ExitButtonSize = {(AdminUserButtonSize.x * 2) + AdminUserButtonGap, 60};

        FadeTransition();
        
        BeginDrawing();
        ClearBackground(GetColor(0x181818));
// ----------------------------------------------------------------------------------------------------    
        BeginShaderMode(shader);
        rlPushMatrix();
        rlLoadIdentity();

        for (int i = 0; i < MAX_TRIANGLES; i++) {
            // Move diagonally up-right
            triangles[i].position.x += triangles[i].speed;
            triangles[i].position.y -= triangles[i].speed;

            // Rotate the triangle
            triangles[i].angle += triangles[i].rotationSpeed;

            // Respawn if out of view
            if (triangles[i].position.x > SCREEN_WIDTH + 100 || triangles[i].position.y < -100) {
                triangles[i].position = (Vector2){
                    GetRandomValue(-200, 200),
                    GetRandomValue(SCREEN_HEIGHT, SCREEN_HEIGHT + 400)
                };
                triangles[i].size = GetRandomValue(30, 60);
                triangles[i].speed = GetRandomValue(10, 30) / 10.0f;
                triangles[i].angle = GetRandomValue(0, 360);
                triangles[i].rotationSpeed = GetRandomValue(5, 20) / 10.0f;

                triangles[i].c1 = RandomColor();
                triangles[i].c2 = RandomColor();
                triangles[i].c3 = RandomColor();
            }

            DrawRotatedGradientTriangle(triangles[i]);
        }

        rlPopMatrix();
        EndShaderMode();
// ----------------------------------------------------------------------------------------------------
        int BGMIndex = (GetTimeOfDay() - 1);

        if (CurrentBGMIndex == -1) {
            CurrentBGMIndex = BGMIndex;
            PlayMusicStream(BackgroundMusics[CurrentBGMIndex]);
        }

        UpdateMusicStream(BackgroundMusics[CurrentBGMIndex]);
        if (BGMIndex != CurrentBGMIndex) ShouldChangeBGM = true;
        
        if (!IsMusicStreamPlaying(BackgroundMusics[CurrentBGMIndex])) {
            if (ShouldChangeBGM) {
                StopMusicStream(BackgroundMusics[CurrentBGMIndex]);
                CurrentBGMIndex = (GetTimeOfDay() - 1);
                PlayMusicStream(BackgroundMusics[CurrentBGMIndex]);
                ShouldChangeBGM = false;
            } else {
                PlayMusicStream(BackgroundMusics[CurrentBGMIndex]);
            }
        }
// ----------------------------------------------------------------------------------------------------
        if (CurrentMenu == MENU_WELCOME) {
            DrawTextEx(GIFont, TitleMessage.TextFill, TitleMessage.TextPosition, TitleMessage.FontData.x, TitleMessage.FontData.y, WHITE);
            DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

            Rectangle AdminButton = {(SCREEN_WIDTH / 2) - AdminUserButtonSize.x - (AdminUserButtonGap / 2), 340, AdminUserButtonSize.x, AdminUserButtonSize.y};
            Rectangle UserButton = {AdminButton.x + AdminUserButtonSize.x + AdminUserButtonGap, AdminButton.y, AdminUserButtonSize.x, AdminUserButtonSize.y};
            Rectangle AboutButton = {(SCREEN_WIDTH / 2) - (AboutButtonSize.x / 2), (AdminButton.y + AdminUserButtonSize.y) + AdminUserButtonGap, AboutButtonSize.x, AboutButtonSize.y};
            Rectangle ExitButton = {(SCREEN_WIDTH / 2) - (ExitButtonSize.x / 2), (AboutButton.y + AboutButton.height) + (AdminUserButtonGap / 2), ExitButtonSize.x, ExitButtonSize.y};
            
            Rectangle Buttons[4] = {AdminButton, UserButton, AboutButton, ExitButton};
            Color ButtonColor[4] = { 0 }, TextColor[4] = { 0 };
            const char *ButtonTexts[] = {
                "~ [1] ADMINISTRATOR ~",
                "~ [2] PENGGUNA ~",
                "Peran: Pemerintah",
                "Peran: Vendor/Catering",
                "Tentang D'Magis...",
                "Keluar..."
            };
            
            for (int i = 0; i < 4; i++) {
                if (CheckCollisionPointRec(Mouse, Buttons[i])) {
                    HoverTransition[i] = CustomLerp(HoverTransition[i], 1.0f, 0.1f);
                    
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        if (i == 0) { if (ST_Admin.SignInChance >= 0) NextMenu = MENU_WELCOME_GOVERNMENT_SignIn; }
                        if (i == 1) NextMenu = MENU_WELCOME_VENDOR;
                        if (i == 2) NextMenu = MENU_ABOUT;
                        if (i == 3) NextMenu = MENU_EXIT;
                        
                        PlaySound(ButtonClickSFX);
                        Transitioning = true;
                        ScreenFade = 1.0f;
                    }

                } else HoverTransition[i] = CustomLerp(HoverTransition[i], 0.0f, 0.1f);
                
                if (i == 0) {
                    if (ST_Admin.SignInChance >= 0) {
                        ButtonColor[i] = (HoverTransition[i] > 0.5f) ? SKYBLUE : DARKBLUE;
                        TextColor[i] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
                    } else {
                        ButtonColor[i] = (HoverTransition[i] > 0.5f) ? GRAY : DARKGRAY;
                        TextColor[i] = BLACK;
                    }
                } else if (i == 1) {
                    ButtonColor[i] = (HoverTransition[i] > 0.5f) ? PURPLE : DARKPURPLE;
                    TextColor[i] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
                } else if (i == 2) {
                    ButtonColor[i] = (HoverTransition[i] > 0.5f) ? GREEN : WHITE;
                    TextColor[i] = (HoverTransition[i] > 0.5f) ? WHITE : GREEN;
                } else if (i == 3) {
                    ButtonColor[i] = (HoverTransition[i] > 0.5f) ? RED : WHITE;
                    TextColor[i] = (HoverTransition[i] > 0.5f) ? WHITE : RED;
                }

                float scale = 1.0f + HoverTransition[i] * 0.1f;
                
                float newWidth = Buttons[i].width * scale;
                float newHeight = Buttons[i].height * scale;
                float newX = Buttons[i].x - (newWidth - Buttons[i].width) / 2;
                float newY = Buttons[i].y - (newHeight - Buttons[i].height) / 2;
                
                DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, ButtonColor[i]);
                
                if (i < 2) {
                    Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[i], (FONT_P * scale), SPACING_WIDER);
                    int textX = newX + (newWidth / 2) - (textWidth.x / 2);
                    int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
                    DrawTextEx(GIFont, ButtonTexts[i], (Vector2){textX, (textY) - 20}, (FONT_P * scale), SPACING_WIDER, TextColor[i]);

                    textWidth = MeasureTextEx(GIFont, ButtonTexts[i + 2], (FONT_P * scale), SPACING_WIDER);
                    textX = newX + (newWidth / 2) - (textWidth.x / 2);
                    textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
                    DrawTextEx(GIFont, ButtonTexts[i + 2], (Vector2){textX, (textY + 40) - 20}, (FONT_P * scale), SPACING_WIDER, TextColor[i]);

                    if (i == 0 && ST_Admin.SignInChance < 0) {
                        char CooldownTimer[256] = { 0 };
                        snprintf(CooldownTimer, sizeof(CooldownTimer), "Tunggu %d detik...", ST_Admin.RemainingTime);

                        textWidth = MeasureTextEx(GIFont, CooldownTimer, (FONT_P * scale), SPACING_WIDER);
                        textX = newX + (newWidth / 2) - (textWidth.x / 2);
                        textY = newY + (newHeight / 2) - (FONT_P * scale / 2);

                        if (ST_Admin.RemainingTime > 0) { DrawTextEx(GIFont, CooldownTimer, (Vector2){textX, textY + 100}, (FONT_P * scale), SPACING_WIDER, WHITE); }
                        else { DrawTextEx(GIFont, CooldownTimer, (Vector2){textX, (textY + 40) - 20}, (FONT_P * scale), SPACING_WIDER, Fade(BLACK, 0.0f)); }
                    } 
                } else if (i == 2) {
                    Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[4], (FONT_P * scale), SPACING_WIDER);
                    int textX = newX + (newWidth / 2) - (textWidth.x / 2);
                    int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
                    DrawTextEx(GIFont, ButtonTexts[4], (Vector2){textX, textY}, (FONT_P * scale), SPACING_WIDER, TextColor[i]);
                } else if (i == 3) {
                    Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[5], (FONT_P * scale), SPACING_WIDER);
                    int textX = newX + (newWidth / 2) - (textWidth.x / 2);
                    int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
                    DrawTextEx(GIFont, ButtonTexts[5], (Vector2){textX, textY}, (FONT_P * scale), SPACING_WIDER, TextColor[i]);
                }
            }
        }

        else if (CurrentMenu == MENU_WELCOME_GOVERNMENT_SignIn) {
            DrawTextEx(GIFont, TitleMessage.TextFill, TitleMessage.TextPosition, TitleMessage.FontData.x, TitleMessage.FontData.y, WHITE);
            DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

            const char *DataTexts[] = {
                "Silakan untuk menyertakan data pribadi Anda sebagai Pemerintah terlebih dahulu...",
                "Nama/Kode Pemerintah",
                "Kata Sandi",
                
                "Maks.: 30 Karakter (3 u/ Kode)",
                "Maks.: 30 Karakter",
                
                "Mohon maaf, terdapat input data pribadi Anda yang salah. Silakan untuk disertakan ulang kembali...",
                
                "Anda masih menyertakan data yang salah.",
                "Kesempatan terakhir telah diberikan kepada Anda untuk mencoba masuk sebagai Administrator...",
                
                "Anda sudah TIGA (3) kali gagal masuk sebagai Administrator.",
                "Anda akan diarahkan kembali ke menu utama dan akses masuk sebagai Administrator akan",
                "DIBLOKIR selama beberapa waktu ke depan!"
            };
            const char *ButtonTexts[2] = {"Reset...", "Sign In..."};
            
            Rectangle ResetButton = {(SCREEN_WIDTH / 2) - ResetSubmitButtonSize.x - (ResetSubmitButtonGap / 2), WelcomeInputList[1].Box.y + ButtonsGap, ResetSubmitButtonSize.x, ResetSubmitButtonSize.y};
            Rectangle SubmitButton = {ResetButton.x + ResetSubmitButtonSize.x + ResetSubmitButtonGap, ResetButton.y, ResetSubmitButtonSize.x, ResetSubmitButtonSize.y};
            Rectangle Buttons[2] = {ResetButton, SubmitButton};
            Color ButtonColor[2], TextColor[2];

            TextObject InputBoxHelp[2] = {
                CreateTextObject(GIFont, DataTexts[1], (WelcomeInputBoxSize.y / 2), SPACING_WIDER, (Vector2){WelcomeInputList[0].Box.x + 10, WelcomeInputList[0].Box.y + 15}, 0, false),
                CreateTextObject(GIFont, DataTexts[2], (WelcomeInputBoxSize.y / 2), SPACING_WIDER, (Vector2){WelcomeInputList[1].Box.x + 10, WelcomeInputList[1].Box.y + 15}, 0, false)
            };
            TextObject MaxCharsInfo[2] = {
                CreateTextObject(GIFont, DataTexts[3], (WelcomeInputBoxSize.y / 3), SPACING_WIDER, (Vector2){WelcomeInputList[0].Box.x + WelcomeInputList[0].Box.width - (MeasureTextEx(GIFont, DataTexts[3], (WelcomeInputBoxSize.y / 3), SPACING_WIDER).x) - 15, WelcomeInputList[0].Box.y + (WelcomeInputBoxSize.y / 3)}, 0, false),
                CreateTextObject(GIFont, DataTexts[4], (WelcomeInputBoxSize.y / 3), SPACING_WIDER, (Vector2){WelcomeInputList[1].Box.x + WelcomeInputList[1].Box.width - (MeasureTextEx(GIFont, DataTexts[4], (WelcomeInputBoxSize.y / 3), SPACING_WIDER).x) - 15, WelcomeInputList[1].Box.y + (WelcomeInputBoxSize.y / 3)}, 0, false)
            };
            
            TextObject SignInMessage = CreateTextObject(GIFont, DataTexts[0], FONT_H3, SPACING_WIDER, (Vector2){0, 280}, 0, true);
            TextObject FirstErrorMessage = CreateTextObject(GIFont, DataTexts[5], FONT_H3, SPACING_WIDER, (Vector2){0, SignInMessage.TextPosition.y}, 40, true);
            TextObject SecondErrorMessage[2] = {
                CreateTextObject(GIFont, DataTexts[6], FONT_H3, SPACING_WIDER, (Vector2){0, SignInMessage.TextPosition.y}, 40, true),
                CreateTextObject(GIFont, DataTexts[7], FONT_H3, SPACING_WIDER, (Vector2){0, SecondErrorMessage[0].TextPosition.y}, 40, true),
            };
            TextObject ThirdErrorMessage[3] = {
                CreateTextObject(GIFont, DataTexts[8], FONT_H3, SPACING_WIDER, (Vector2){0, SignInMessage.TextPosition.y}, 0, true),
                CreateTextObject(GIFont, DataTexts[9], FONT_H3, SPACING_WIDER, (Vector2){0, ThirdErrorMessage[0].TextPosition.y}, 40, true),
                CreateTextObject(GIFont, DataTexts[10], FONT_H3, SPACING_WIDER, (Vector2){0, ThirdErrorMessage[1].TextPosition.y}, 40, true),
            };
            
            if (ST_Admin.SignInChance < 0) {
                for (int i = 0; i < 3; i++) { DrawTextEx(GIFont, ThirdErrorMessage[i].TextFill, ThirdErrorMessage[i].TextPosition, ThirdErrorMessage[i].FontData.x, ThirdErrorMessage[i].FontData.y, RED); }
                CurrentFrameTime += GetFrameTime();

                if (CurrentFrameTime >= 3.0f) {
                    PreviousMenu = MENU_WELCOME;
                    NextMenu = MENU_WELCOME;
                    CurrentMenu = NextMenu;

                    ST_Admin.AllValid = false; ST_Admin.LastTime = ST_Admin.CurrentTime; ST_Admin.CooldownTime *= 1;
                    UpdateLastSignInTime(&ST_Admin, "/data/"ADMIN_SAVEFILE);

                    HasClicked[0] = false; CheckForSignInClick = false;
                    Transitioning = true;
                    ScreenFade = 1.0f;
                    
                    CurrentFrameTime = 0;
                    for (int i = 0; i < 2; i++) { WelcomeInputList[i].IsValid = true; }
                }
            } else {
                PreviousMenu = MENU_WELCOME;
                DrawPreviousMenuButton();
                
                DrawTextEx(GIFont, SignInMessage.TextFill, SignInMessage.TextPosition, SignInMessage.FontData.x, SignInMessage.FontData.y, PINK);
                DrawRectangleRec(WelcomeInputList[0].Box, GRAY);
                DrawRectangleRec(WelcomeInputList[1].Box, GRAY);

                for (int i = 3; i < 5; i++) {
                    if (CheckCollisionPointRec(Mouse, Buttons[i - 3])) {
                        HoverTransition[i] = CustomLerp(HoverTransition[i], 1.0f, 0.1f);
                        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                            if (i == 3) ResetInputs(WelcomeInputList, (Vector2){0, 2});
                            if (i == 4) {
                                CheckForSignInClick = true;
                                ST_Admin.AllValid = true;

                                for (int j = 0; j < 2; j++) {
                                    WelcomeInputList[j].IsValid = (strlen(WelcomeInputList[j].Text) >= 3);
                                    if (!WelcomeInputList[j].IsValid) ST_Admin.AllValid = false;
                                } HasClicked[0] = true;
                            }
                            
                            PlaySound(ButtonClickSFX);
                            Transitioning = true;
                            ScreenFade = 1.0f;
                        }
                    } else HoverTransition[i] = CustomLerp(HoverTransition[i], 0.0f, 0.1f);
                    
                    if (i == 3) { // Reset button
                        ButtonColor[i - 3] = (HoverTransition[i] > 0.5f) ? RED : LIGHTGRAY;
                        TextColor[i - 3] = (HoverTransition[i] > 0.5f) ? WHITE : BLACK;
                    } else if (i == 4) { // Submit button
                        ButtonColor[i - 3] = (HoverTransition[i] > 0.5f) ? GREEN : LIGHTGRAY;
                        TextColor[i - 3] = (HoverTransition[i] > 0.5f) ? WHITE : BLACK;
                    }

                    float scale = 1.0f + HoverTransition[i] * 0.1f;
                    
                    float newWidth = Buttons[i - 3].width * scale;
                    float newHeight = Buttons[i - 3].height * scale;
                    float newX = Buttons[i - 3].x - (newWidth - Buttons[i - 3].width) / 2;
                    float newY = Buttons[i - 3].y - (newHeight - Buttons[i - 3].height) / 2;
                    
                    DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, ButtonColor[i - 3]);
                    
                    Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[i - 3], (FONT_P * scale), SPACING_WIDER);
                    int textX = newX + (newWidth / 2) - (textWidth.x / 2);
                    int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
                    DrawTextEx(GIFont, ButtonTexts[i - 3], (Vector2){textX, textY}, (FONT_P * scale), SPACING_WIDER, TextColor[i - 3]);
                }
                
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    ActiveBox = -1;  // Reset active box
                    for (int i = 0; i < 2; i++) {
                        if (CheckCollisionPointRec(Mouse, WelcomeInputList[i].Box)) {
                            ActiveBox = i;
                            break;
                        }
                    }
                }

                if (ActiveBox != -1) {
                    int Key = GetCharPressed();
                    while (Key > 0) {
                        if (strlen(WelcomeInputList[ActiveBox].Text) < MAX_INPUT_LENGTH) {
                            int len = strlen(WelcomeInputList[ActiveBox].Text);
                            WelcomeInputList[ActiveBox].Text[len] = (char)Key;
                            WelcomeInputList[ActiveBox].Text[len + 1] = '\0';
                            if (ActiveBox % 2 != 0) {
                                WelcomeInputList[ActiveBox].MaskedPassword[len - 1] = '*';
                                WelcomeInputList[ActiveBox].MaskedPassword[len] = WelcomeInputList[ActiveBox].Text[len];
                                WelcomeInputList[ActiveBox].MaskedPassword[len + 1] = '\0';
                            }
                        } Key = GetCharPressed();
                    }

                    OccuringLastCharacter += GetFrameTime();
                    if (OccuringLastCharacter >= 1.0f) {
                        WelcomeInputList[ActiveBox].MaskedPassword[strlen(WelcomeInputList[ActiveBox].MaskedPassword) - 1] = '*';
                        OccuringLastCharacter = 0;
                    }

                    if (IsKeyPressed(KEY_BACKSPACE) && strlen(WelcomeInputList[ActiveBox].Text) > 0) {
                        WelcomeInputList[ActiveBox].Text[strlen(WelcomeInputList[ActiveBox].Text) - 1] = '\0';
                        if (ActiveBox % 2 != 0) WelcomeInputList[ActiveBox].MaskedPassword[strlen(WelcomeInputList[ActiveBox].MaskedPassword) - 1] = '\0';
                    } else if (IsKeyDown(KEY_BACKSPACE) && strlen(WelcomeInputList[ActiveBox].Text) > 0) {
                        CurrentFrameTime += GetFrameTime();
                        if (CurrentFrameTime >= 0.5f) {
                            WelcomeInputList[ActiveBox].Text[strlen(WelcomeInputList[ActiveBox].Text) - 1] = '\0';
                            if (ActiveBox % 2 != 0) WelcomeInputList[ActiveBox].MaskedPassword[strlen(WelcomeInputList[ActiveBox].MaskedPassword) - 1] = '\0';
                        }
                    }
                    
                    if (IsKeyReleased(KEY_BACKSPACE)) CurrentFrameTime = 0;
                }

                FrameCounter++; // Increment frame counter for cursor blinking
                for (int i = 0; i < 2; i++) {
                    Color boxColor = GRAY;
                    if (!WelcomeInputList[i].IsValid && HasClicked[0]) { boxColor = RED; }
                    
                    if (i == ActiveBox) boxColor = SKYBLUE; // Active input
                    else if (CheckCollisionPointRec(Mouse, WelcomeInputList[i].Box)) boxColor = YELLOW; // Hover effect

                    DrawRectangleRec(WelcomeInputList[i].Box, boxColor);
                    if (i % 2 == 0) DrawTextEx(GIFont, WelcomeInputList[i].Text, (Vector2){WelcomeInputList[i].Box.x + 10, WelcomeInputList[i].Box.y + 15}, (WelcomeInputBoxSize.y / 2), SPACING_WIDER, BLACK);
                    else DrawTextEx(GIFont, WelcomeInputList[i].MaskedPassword, (Vector2){WelcomeInputList[i].Box.x + 10, WelcomeInputList[i].Box.y + 15}, (WelcomeInputBoxSize.y / 2), SPACING_WIDER, BLACK);

                    if (i == ActiveBox && (FrameCounter / CURSOR_BLINK_SPEED) % 2 == 0) {
                        int textWidth;
                        if (ActiveBox % 2 == 0) textWidth = MeasureTextEx(GIFont, WelcomeInputList[i].Text, (WelcomeInputBoxSize.y / 2), SPACING_WIDER).x;
                        else textWidth = MeasureTextEx(GIFont, WelcomeInputList[i].MaskedPassword, (WelcomeInputBoxSize.y / 2), SPACING_WIDER).x;
                        DrawTextEx(GIFont, "|", (Vector2){WelcomeInputList[i].Box.x + 10 + textWidth, WelcomeInputList[i].Box.y + 10}, (WelcomeInputBoxSize.y / 2) + 10, SPACING_WIDER, BLACK);
                    }
                    
                    DrawTextEx(GIFont, MaxCharsInfo[i].TextFill, MaxCharsInfo[i].TextPosition, MaxCharsInfo[i].FontData.x, MaxCharsInfo[i].FontData.y, BLACK);
                    if (strlen(WelcomeInputList[i].Text) == 0 && i != ActiveBox) {
                        DrawTextEx(GIFont, InputBoxHelp[i].TextFill, InputBoxHelp[i].TextPosition, InputBoxHelp[i].FontData.x, InputBoxHelp[i].FontData.y, Fade(BLACK, 0.6));
                    }
                }

                if (HasClicked[0]) {
                    char Username[256] = { 0 }, Codename[256] = { 0 }, Password[256] = { 0 };
                    strcpy(Username, WelcomeInputList[0].Text); strcpy(Codename, WelcomeInputList[0].Text); strcpy(Password, WelcomeInputList[1].Text);
                    EXDRO(Username, GetEncryptionKey(ENCRYPTION_KEY));
                    EXDRO(Codename, GetEncryptionKey(ENCRYPTION_KEY));
                    EXDRO(Password, GetEncryptionKey(ENCRYPTION_KEY));
                    
                    if      (strlen(Codename) > 3)  { if (strcmp(Username, ADMIN_USERNAME) != 0 || (strcmp(Password, ADMIN_PASSWORD_V1) != 0 && strcmp(Password, ADMIN_PASSWORD_V2) != 0)) ST_Admin.AllValid = false; }
                    else if (strlen(Username) == 3) { if (strcmp(Codename, ADMIN_CODENAME) != 0 || (strcmp(Password, ADMIN_PASSWORD_V1) != 0 && strcmp(Password, ADMIN_PASSWORD_V2) != 0)) ST_Admin.AllValid = false; }

                    if (ST_Admin.AllValid) {
                        PreviousMenu = CurrentMenu;
                        NextMenu = MENU_WELCOME_GOVERNMENT_MainMenuTransition;
                        CurrentMenu = NextMenu;

                        ST_Admin.CooldownTime = 15;
                        ST_Admin.LastTime = ST_Admin.CurrentTime;
                        UpdateLastSignInTime(&ST_Admin, "/data/"ADMIN_SAVEFILE);

                        HasClicked[0] = false;
                        Transitioning = true;
                        ScreenFade = 1.0f;
                    } else {
                        if (CheckForSignInClick) {
                            ST_Admin.SignInChance--;
                            ST_Admin.LastTime = ST_Admin.CurrentTime;
                            UpdateLastSignInTime(&ST_Admin, "/data/"ADMIN_SAVEFILE);
                            
                            CheckForSignInClick = false;
                        }

                        if (ST_Admin.SignInChance > 0 && ST_Admin.SignInChance < 3) {
                            DrawTextEx(GIFont, FirstErrorMessage.TextFill, FirstErrorMessage.TextPosition, FirstErrorMessage.FontData.x, FirstErrorMessage.FontData.y, RED);
                        } else if (ST_Admin.SignInChance == 0) {
                            DrawTextEx(GIFont, FirstErrorMessage.TextFill, FirstErrorMessage.TextPosition, FirstErrorMessage.FontData.x, FirstErrorMessage.FontData.y, Fade(BLACK, 0));
                            for (int i = 0; i < 2; i++) { DrawTextEx(GIFont, SecondErrorMessage[i].TextFill, SecondErrorMessage[i].TextPosition, SecondErrorMessage[i].FontData.x, SecondErrorMessage[i].FontData.y, RED); }
                        }
                    }
                }
            }
        } else if (CurrentMenu == MENU_WELCOME_GOVERNMENT_MainMenuTransition) {
            DrawTextEx(GIFont, TitleMessage.TextFill, TitleMessage.TextPosition, TitleMessage.FontData.x, TitleMessage.FontData.y, WHITE);
            DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

            const char *SuccessMessage[2] = {
                "Anda telah berhasil masuk sebagai Administrator, bertugas sebagai Pemerintah.",
                "Sesaat setelah ini, Anda akan segera diarahkan ke menu utama Administrator. Selamat bekerja, Pemerintah setempat!"
            };
            TextObject FirstInfoMessage = CreateTextObject(GIFont, SuccessMessage[0], FONT_H2, SPACING_WIDER, (Vector2){0, 520}, 0, true);
            TextObject SecondInfoMessage = CreateTextObject(GIFont, SuccessMessage[1], FONT_H3, SPACING_WIDER, (Vector2){0, FirstInfoMessage.TextPosition.y}, 60, true);

            DrawTextEx(GIFont, FirstInfoMessage.TextFill, FirstInfoMessage.TextPosition, FirstInfoMessage.FontData.x, FirstInfoMessage.FontData.y, GREEN);
            DrawTextEx(GIFont, SecondInfoMessage.TextFill, SecondInfoMessage.TextPosition, SecondInfoMessage.FontData.x, SecondInfoMessage.FontData.y, SKYBLUE);

            CurrentFrameTime += GetFrameTime();
            if (CurrentFrameTime >= 3.0f) {
                PreviousMenu = CurrentMenu;
                NextMenu = MENU_MAIN_GOVERNMENT;
                CurrentMenu = NextMenu;

                Transitioning = true;
                ScreenFade = 1.0f;
                CurrentFrameTime = 0;
            }
            
            ST_Admin.SignInChance = 3;
            UpdateLastSignInTime(&ST_Admin, "/data/"ADMIN_SAVEFILE);
        }

        else if (CurrentMenu == MENU_WELCOME_VENDOR) {
            DrawTextEx(GIFont, TitleMessage.TextFill, TitleMessage.TextPosition, TitleMessage.FontData.x, TitleMessage.FontData.y, WHITE);
            DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

            PreviousMenu = MENU_WELCOME;
            DrawPreviousMenuButton();

            Rectangle SignUpButton = {(SCREEN_WIDTH / 2) - AdminUserButtonSize.x - (AdminUserButtonGap / 2), 400, AdminUserButtonSize.x, AdminUserButtonSize.y};
            Rectangle SignInButton = {SignUpButton.x + AdminUserButtonSize.x + AdminUserButtonGap, 400, AdminUserButtonSize.x, AdminUserButtonSize.y};
            
            Rectangle Buttons[2] = {SignUpButton, SignInButton};
            Color ButtonColor[2] = { 0 }, TextColor[2] = { 0 };
            const char *ButtonTexts[] = {
                "~ [1] Daftar (Sign Up) ~",
                "~ [2] Masuk (Sign In) ~",
                "Daftar sebagai Vendor/Catering\nbaru dan mulai berkontribusi\nuntuk para siswa!",
                "Masuk sebagai Vendor/Catering\ndan siap bekerja kembali!",
            };
            
            for (int i = 5; i < 7; i++) {
                if (CheckCollisionPointRec(Mouse, Buttons[i - 5])) {
                    HoverTransition[i] = CustomLerp(HoverTransition[i], 1.0f, 0.1f);
                    
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        if (i == 5) { NextMenu = MENU_WELCOME_VENDOR_SignUp; }
                        if (i == 6) { if (ST_User.SignInChance >= 0) NextMenu = MENU_WELCOME_VENDOR_SignIn; }
                        
                        PlaySound(ButtonClickSFX);
                        Transitioning = true;
                        ScreenFade = 1.0f;
                    }
                } else HoverTransition[i] = CustomLerp(HoverTransition[i], 0.0f, 0.1f);
                
                if (i == 5) {
                    ButtonColor[i - 5] = (HoverTransition[i] > 0.5f) ? GOLD : BROWN;
                    TextColor[i - 5] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
                } else if (i == 6) {
                    if (ST_User.SignInChance >= 0) {
                        ButtonColor[i - 5] = (HoverTransition[i] > 0.5f) ? GREEN : DARKGREEN;
                        TextColor[i - 5] = (HoverTransition[i] > 0.5f) ? BLACK : WHITE;
                    } else {
                        ButtonColor[i - 5] = (HoverTransition[i] > 0.5f) ? GRAY : DARKGRAY;
                        TextColor[i - 5] = BLACK;
                    }
                }

                float scale = 1.0f + HoverTransition[i] * 0.1f;
                
                float newWidth = Buttons[i - 5].width * scale;
                float newHeight = Buttons[i - 5].height * scale;
                float newX = Buttons[i - 5].x - (newWidth - Buttons[i - 5].width) / 2;
                float newY = Buttons[i - 5].y - (newHeight - Buttons[i - 5].height) / 2;
                
                DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, ButtonColor[i - 5]);
                
                Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[i - 5], (FONT_P * scale), SPACING_WIDER);
                int textX = newX + (newWidth / 2) - (textWidth.x / 2);
                int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
                DrawTextEx(GIFont, ButtonTexts[i - 5], (Vector2){textX, textY - (textY / 4)}, (FONT_P * scale), SPACING_WIDER, TextColor[i - 5]);

                textWidth = MeasureTextEx(GIFont, ButtonTexts[(i + 2) - 5], (FONT_H5 * scale), SPACING_WIDER);
                textX = newX + (newWidth / 2) - (textWidth.x / 2);
                textY = newY + (newHeight / 2) - (FONT_H5 * scale / 2);
                DrawTextEx(GIFont, ButtonTexts[(i + 2) - 5], (Vector2){textX, (textY + 60) - (textY / 4)}, (FONT_H5 * scale), SPACING_WIDER, TextColor[i - 5]);

                if (i == 6 && ST_User.SignInChance < 0) {
                    char CooldownTimer[256] = { 0 };
                    snprintf(CooldownTimer, sizeof(CooldownTimer), "Tunggu %d detik...", ST_User.RemainingTime);

                    textWidth = MeasureTextEx(GIFont, CooldownTimer, (FONT_P * scale), SPACING_WIDER);
                    textX = newX + (newWidth / 2) - (textWidth.x / 2);
                    textY = newY + (newHeight / 2) - (FONT_P * scale / 2);

                    if (ST_User.RemainingTime > 0) { DrawTextEx(GIFont, CooldownTimer, (Vector2){textX, textY + 100}, (FONT_P * scale), SPACING_WIDER, WHITE); }
                    else { DrawTextEx(GIFont, CooldownTimer, (Vector2){textX, textY + 40}, (FONT_P * scale), SPACING_WIDER, Fade(BLACK, 0.0f)); }
                }
            }
        }
        
        else if (CurrentMenu == MENU_WELCOME_VENDOR_SignUp) {
            DrawTextEx(GIFont, TitleMessage.TextFill, TitleMessage.TextPosition, TitleMessage.FontData.x, TitleMessage.FontData.y, WHITE);
            DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

            PreviousMenu = MENU_WELCOME_VENDOR;
            DrawPreviousMenuButton();

            const char *DataTexts[] = {
                "Silakan untuk menyertakan data pribadi BARU Anda sebagai Vendor (atau Catering) terlebih dahulu...",
                "Teruntuk proses pengisiannya, diharapkan dibuat CUKUP MUDAH untuk diingat oleh Anda pribadi!",
                "Nama/Kode Vendor (atau Catering)",
                "Kata Sandi",
                
                "Maks.: 30 Karakter (3 u/ Kode)",
                "Maks.: 30 Karakter",

                "Mohon untuk memastikan kembali untuk semua hasil input Anda telah memenuhi kriteria yang diminta!"
            };
            const char *ButtonTexts[2] = {"Reset...", "Sign Up..."};
            
            Rectangle ResetButton = {(SCREEN_WIDTH / 2) - ResetSubmitButtonSize.x - (ResetSubmitButtonGap / 2), WelcomeInputList[1 + 2].Box.y + ButtonsGap, ResetSubmitButtonSize.x, ResetSubmitButtonSize.y};
            Rectangle SubmitButton = {ResetButton.x + ResetSubmitButtonSize.x + ResetSubmitButtonGap, ResetButton.y, ResetSubmitButtonSize.x, ResetSubmitButtonSize.y};
            Rectangle Buttons[2] = {ResetButton, SubmitButton};
            Color ButtonColor[2] = { 0 }, TextColor[2] = { 0 };

            TextObject InputBoxHelp[2] = {
                CreateTextObject(GIFont, DataTexts[2], (WelcomeInputBoxSize.y / 2), SPACING_WIDER, (Vector2){WelcomeInputList[0 + 2].Box.x + 10, WelcomeInputList[0 + 2].Box.y + 15}, 0, false),
                CreateTextObject(GIFont, DataTexts[3], (WelcomeInputBoxSize.y / 2), SPACING_WIDER, (Vector2){WelcomeInputList[1 + 2].Box.x + 10, WelcomeInputList[1 + 2].Box.y + 15}, 0, false)
            };
            TextObject MaxCharsInfo[2] = {
                CreateTextObject(GIFont, DataTexts[4], (WelcomeInputBoxSize.y / 3), SPACING_WIDER, (Vector2){WelcomeInputList[0 + 2].Box.x + WelcomeInputList[0 + 2].Box.width - (MeasureTextEx(GIFont, DataTexts[4], (WelcomeInputBoxSize.y / 3), SPACING_WIDER).x) - 15, WelcomeInputList[0 + 2].Box.y + (WelcomeInputBoxSize.y / 3)}, 0, false),
                CreateTextObject(GIFont, DataTexts[5], (WelcomeInputBoxSize.y / 3), SPACING_WIDER, (Vector2){WelcomeInputList[1 + 2].Box.x + WelcomeInputList[1 + 2].Box.width - (MeasureTextEx(GIFont, DataTexts[5], (WelcomeInputBoxSize.y / 3), SPACING_WIDER).x) - 15, WelcomeInputList[1 + 2].Box.y + (WelcomeInputBoxSize.y / 3)}, 0, false)
            };
            
            TextObject SignUpMessage[2] = {
                CreateTextObject(GIFont, DataTexts[0], FONT_H3, SPACING_WIDER, (Vector2){0, 280}, 0, true),
                CreateTextObject(GIFont, DataTexts[1], FONT_H3, SPACING_WIDER, SignUpMessage[0].TextPosition, 40, true),
            };
            TextObject ErrorMessage = CreateTextObject(GIFont, DataTexts[6], FONT_H3, SPACING_WIDER, SignUpMessage[1].TextPosition, 60, true);
            
            for (int i = 0; i < 2; i++) {
                DrawTextEx(GIFont, SignUpMessage[i].TextFill, SignUpMessage[i].TextPosition, SignUpMessage[i].FontData.x, SignUpMessage[i].FontData.y, (i % 2 == 0) ? ORANGE : PINK);
            };
            
            DrawRectangleRec(WelcomeInputList[0 + 2].Box, GRAY);
            DrawRectangleRec(WelcomeInputList[1 + 2].Box, GRAY);

            for (int i = 7; i < 9; i++) {
                if (CheckCollisionPointRec(Mouse, Buttons[i - 7])) {
                    HoverTransition[i] = CustomLerp(HoverTransition[i], 1.0f, 0.1f);
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        if (i == 7) ResetInputs(WelcomeInputList, (Vector2){2, 4});
                        if (i == 8) {
                            CheckForSignInClick = true;
                            ST_User.AllValid = true;

                            for (int j = 0; j < 2; j++) {
                                WelcomeInputList[j + 2].IsValid = (strlen(WelcomeInputList[j + 2].Text) >= 3);
                                if (!WelcomeInputList[j + 2].IsValid) ST_User.AllValid = false;
                            } HasClicked[1] = true;
                        }
                        
                        PlaySound(ButtonClickSFX);
                        Transitioning = true;
                        ScreenFade = 1.0f;
                    }
                } else HoverTransition[i] = CustomLerp(HoverTransition[i], 0.0f, 0.1f);
                
                if (i == 7) { // Reset button
                    ButtonColor[i - 7] = (HoverTransition[i] > 0.5f) ? RED : LIGHTGRAY;
                    TextColor[i - 7] = (HoverTransition[i] > 0.5f) ? WHITE : BLACK;
                } else if (i == 8) { // Submit button
                    ButtonColor[i - 7] = (HoverTransition[i] > 0.5f) ? GREEN : LIGHTGRAY;
                    TextColor[i - 7] = (HoverTransition[i] > 0.5f) ? WHITE : BLACK;
                }

                float scale = 1.0f + HoverTransition[i] * 0.1f;
                
                float newWidth = Buttons[i - 7].width * scale;
                float newHeight = Buttons[i - 7].height * scale;
                float newX = Buttons[i - 7].x - (newWidth - Buttons[i - 7].width) / 2;
                float newY = Buttons[i - 7].y - (newHeight - Buttons[i - 7].height) / 2;
                
                DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, ButtonColor[i - 7]);
                
                Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[i - 7], (FONT_P * scale), SPACING_WIDER);
                int textX = newX + (newWidth / 2) - (textWidth.x / 2);
                int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
                DrawTextEx(GIFont, ButtonTexts[i - 7], (Vector2){textX, textY}, (FONT_P * scale), SPACING_WIDER, TextColor[i - 7]);
            }
            
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                ActiveBox = -1;  // Reset active box
                for (int i = 0; i < 2; i++) {
                    if (CheckCollisionPointRec(Mouse, WelcomeInputList[i + 2].Box)) {
                        ActiveBox = i + 2;
                        break;
                    }
                }
            }

            if (ActiveBox != -1) {
                int Key = GetCharPressed();
                while (Key > 0) {
                    if (strlen(WelcomeInputList[ActiveBox].Text) < MAX_INPUT_LENGTH) {
                        int len = strlen(WelcomeInputList[ActiveBox].Text);
                        WelcomeInputList[ActiveBox].Text[len] = (char)Key;
                        WelcomeInputList[ActiveBox].Text[len + 1] = '\0';
                        if (ActiveBox % 2 != 0) {
                            WelcomeInputList[ActiveBox].MaskedPassword[len - 1] = '*';
                            WelcomeInputList[ActiveBox].MaskedPassword[len] = WelcomeInputList[ActiveBox].Text[len];
                            WelcomeInputList[ActiveBox].MaskedPassword[len + 1] = '\0';
                        }
                    } Key = GetCharPressed();
                }

                OccuringLastCharacter += GetFrameTime();
                if (OccuringLastCharacter >= 1.0f) {
                    WelcomeInputList[ActiveBox].MaskedPassword[strlen(WelcomeInputList[ActiveBox].MaskedPassword) - 1] = '*';
                    OccuringLastCharacter = 0;
                }

                if (IsKeyPressed(KEY_BACKSPACE) && strlen(WelcomeInputList[ActiveBox].Text) > 0) {
                    WelcomeInputList[ActiveBox].Text[strlen(WelcomeInputList[ActiveBox].Text) - 1] = '\0';
                    if (ActiveBox % 2 != 0) WelcomeInputList[ActiveBox].MaskedPassword[strlen(WelcomeInputList[ActiveBox].MaskedPassword) - 1] = '\0';
                } else if (IsKeyDown(KEY_BACKSPACE) && strlen(WelcomeInputList[ActiveBox].Text) > 0) {
                    CurrentFrameTime += GetFrameTime();
                    if (CurrentFrameTime >= 0.5f) {
                        WelcomeInputList[ActiveBox].Text[strlen(WelcomeInputList[ActiveBox].Text) - 1] = '\0';
                        if (ActiveBox % 2 != 0) WelcomeInputList[ActiveBox].MaskedPassword[strlen(WelcomeInputList[ActiveBox].MaskedPassword) - 1] = '\0';
                    }
                }
                
                if (IsKeyReleased(KEY_BACKSPACE)) CurrentFrameTime = 0;
            }

            FrameCounter++; // Increment frame counter for cursor blinking
            for (int i = 0; i < 2; i++) {
                Color boxColor = GRAY;
                if (!WelcomeInputList[i + 2].IsValid && HasClicked[1]) { boxColor = RED; }
                
                if ((i + 2) == ActiveBox) boxColor = SKYBLUE; // Active input
                else if (CheckCollisionPointRec(Mouse, WelcomeInputList[i + 2].Box)) boxColor = YELLOW; // Hover effect

                DrawRectangleRec(WelcomeInputList[i + 2].Box, boxColor);
                if (i % 2 == 0) DrawTextEx(GIFont, WelcomeInputList[i + 2].Text, (Vector2){WelcomeInputList[i + 2].Box.x + 10, WelcomeInputList[i + 2].Box.y + 15}, (WelcomeInputBoxSize.y / 2), SPACING_WIDER, BLACK);
                else DrawTextEx(GIFont, WelcomeInputList[i + 2].MaskedPassword, (Vector2){WelcomeInputList[i + 2].Box.x + 10, WelcomeInputList[i + 2].Box.y + 15}, (WelcomeInputBoxSize.y / 2), SPACING_WIDER, BLACK);

                if ((i + 2) == ActiveBox && (FrameCounter / CURSOR_BLINK_SPEED) % 2 == 0) {
                    int textWidth = MeasureTextEx(GIFont, WelcomeInputList[i + 2].Text, (WelcomeInputBoxSize.y / 2), SPACING_WIDER).x;
                    if (ActiveBox % 2 == 0) textWidth = MeasureTextEx(GIFont, WelcomeInputList[i + 2].Text, (WelcomeInputBoxSize.y / 2), SPACING_WIDER).x;
                    else textWidth = MeasureTextEx(GIFont, WelcomeInputList[i + 2].MaskedPassword, (WelcomeInputBoxSize.y / 2), SPACING_WIDER).x;
                    DrawTextEx(GIFont, "|", (Vector2){WelcomeInputList[i + 2].Box.x + 10 + textWidth, WelcomeInputList[i + 2].Box.y + 10}, (WelcomeInputBoxSize.y / 2) + 10, SPACING_WIDER, BLACK);
                }
                
                DrawTextEx(GIFont, MaxCharsInfo[i].TextFill, MaxCharsInfo[i].TextPosition, MaxCharsInfo[i].FontData.x, MaxCharsInfo[i].FontData.y, BLACK);
                if (strlen(WelcomeInputList[i + 2].Text) == 0 && (i + 2) != ActiveBox) {
                    DrawTextEx(GIFont, InputBoxHelp[i].TextFill, InputBoxHelp[i].TextPosition, InputBoxHelp[i].FontData.x, InputBoxHelp[i].FontData.y, Fade(BLACK, 0.6));
                }
            }

            if (HasClicked[1]) {
                if (ST_User.AllValid) {
                    PreviousMenu = CurrentMenu;
                    NextMenu = MENU_WELCOME_VENDOR_NewAccountConfirmation;
                    CurrentMenu = NextMenu;

                    HasClicked[1] = false;
                    Transitioning = true;
                    ScreenFade = 1.0f;
                } else {
                    DrawTextEx(GIFont, ErrorMessage.TextFill, ErrorMessage.TextPosition, ErrorMessage.FontData.x, ErrorMessage.FontData.y, RED);
                }
            }
        } else if (CurrentMenu == MENU_WELCOME_VENDOR_NewAccountConfirmation) {
            DrawTextEx(GIFont, TitleMessage.TextFill, TitleMessage.TextPosition, TitleMessage.FontData.x, TitleMessage.FontData.y, WHITE);
            DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

            char InputPassword[32] = { 0 }, MaskedPassword[32] = { 0 };
            strcpy(InputPassword, WelcomeInputList[1 + 2].Text); strcpy(MaskedPassword, WelcomeInputList[1 + 2].Text);
            if (!ShowPassword) {
                for (size_t i = 0; i < strlen(InputPassword); i++) { MaskedPassword[i] = '*'; }
                MaskedPassword[strlen(InputPassword)] = '\0';
            }

            const char *DataTexts[] = {
                "Apakah Anda yakin untuk MENYIMPAN dan MELANJUTKAN proses masuk Anda sebagai Vendor (atau Catering)?",
                "Berikut adalah data input Anda. Demi keamanan, kami akan mensensor Kata Sandi Anda...",

                "Nama/Kode Vendor (atau Catering):",
                "Kata Sandi:",

                "Anda telah berhasil membuat akun BARU Pengguna sebagai Vendor (atau Catering)!",
                "Sessat setelah ini, Anda akan dibawa ke menu Sign In untuk menjalani proses masuk akun...",
                
                "Mohon maaf, akan tetapi data akun Anda sebelumnya sudah ada dalam database kami.",

                "Tekan di mana saja untuk melanjutkan..."
            };
            const char *ButtonTexts[2] = {"Tidak...", "Iya..."}, *ButtonShowPassword[2] = {"[O]", "[-]"};
            
            Rectangle ResetButton = {(SCREEN_WIDTH / 2) - ResetSubmitButtonSize.x - (ResetSubmitButtonGap / 2), WelcomeInputList[1 + 4].Box.y + ButtonsGap, ResetSubmitButtonSize.x, ResetSubmitButtonSize.y};
            Rectangle SubmitButton = {ResetButton.x + ResetSubmitButtonSize.x + ResetSubmitButtonGap, ResetButton.y, ResetSubmitButtonSize.x, ResetSubmitButtonSize.y};

            TextObject SignUpMessage[2] = {
                CreateTextObject(GIFont, DataTexts[0], FONT_H3, SPACING_WIDER, (Vector2){0, 280}, 0, true),
                CreateTextObject(GIFont, DataTexts[1], FONT_H3, SPACING_WIDER, SignUpMessage[0].TextPosition, 40, true)
            };

            TextObject DataConfirmationInfo[6] = {
                CreateTextObject(GIFont, DataTexts[2], FONT_P, SPACING_WIDER, (Vector2){WelcomeInputList[0 + 2].Box.x, WelcomeInputList[0 + 2].Box.y}, 0, false),
                CreateTextObject(GIFont, DataTexts[3], FONT_P, SPACING_WIDER, (Vector2){WelcomeInputList[0 + 2].Box.x, WelcomeInputList[0 + 2].Box.y}, 30, false),

                // Display actual or masked password based on ShowPassword
                CreateTextObject(GIFont, WelcomeInputList[0 + 2].Text, FONT_P, SPACING_WIDER, (Vector2){DataConfirmationInfo[0].TextPosition.x + WelcomeInputList[0 + 2].Box.x, DataConfirmationInfo[0].TextPosition.y}, 0, false),
                CreateTextObject(GIFont, ShowPassword ? InputPassword : MaskedPassword, FONT_P, SPACING_WIDER, (Vector2){DataConfirmationInfo[1].TextPosition.x + WelcomeInputList[1 + 2].Box.x, DataConfirmationInfo[1].TextPosition.y}, 0, false),
            };

            TextObject SuccessMessage[3] = {
                CreateTextObject(GIFont, DataTexts[4], FONT_H2, SPACING_WIDER, (Vector2){0, 520}, 0, true),
                CreateTextObject(GIFont, DataTexts[5], FONT_H3, SPACING_WIDER, SuccessMessage[0].TextPosition, 40, true),
                CreateTextObject(GIFont, DataTexts[7], FONT_H3, SPACING_WIDER, SuccessMessage[1].TextPosition, 60, true)
            };
            TextObject WarningMessage[2] = {
                CreateTextObject(GIFont, DataTexts[6], FONT_H2, SPACING_WIDER, (Vector2){0, 520}, 0, true),
                CreateTextObject(GIFont, DataTexts[7], FONT_H3, SPACING_WIDER, WarningMessage[0].TextPosition, 60, true)
            };

            Rectangle Buttons[3] = {
                ResetButton, SubmitButton,
                (Rectangle){DataConfirmationInfo[3].TextPosition.x + WelcomeInputList[1 + 2].Box.x, DataConfirmationInfo[3].TextPosition.y, 60, 60}
            };
            Color ButtonColor[3] = { 0 }, TextColor[3] = { 0 };

            if (SuccessfulWriteUserAccount && HasClicked[2]) {
                DrawTextEx(GIFont, SuccessMessage[0].TextFill, SuccessMessage[0].TextPosition, SuccessMessage[0].FontData.x, SuccessMessage[0].FontData.y, GREEN);
                DrawTextEx(GIFont, SuccessMessage[1].TextFill, SuccessMessage[1].TextPosition, SuccessMessage[1].FontData.x, SuccessMessage[1].FontData.y, SKYBLUE);
                DrawTextEx(GIFont, SuccessMessage[2].TextFill, SuccessMessage[2].TextPosition, SuccessMessage[2].FontData.x, SuccessMessage[2].FontData.y, WHITE);

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    PreviousMenu = MENU_WELCOME_VENDOR_SignIn;
                    NextMenu = MENU_WELCOME_VENDOR_SignIn;
                    CurrentMenu = NextMenu;

                    PlaySound(ButtonClickSFX);
                    SuccessfulWriteUserAccount = false;
                    HasClicked[2] = false;
                    Transitioning = true;
                    ScreenFade = 1.0f;
                }
            } else if (!SuccessfulWriteUserAccount && HasClicked[2]) {
                DrawTextEx(GIFont, WarningMessage[0].TextFill, WarningMessage[0].TextPosition, WarningMessage[0].FontData.x, WarningMessage[0].FontData.y, RED);
                DrawTextEx(GIFont, WarningMessage[1].TextFill, WarningMessage[1].TextPosition, WarningMessage[1].FontData.x, WarningMessage[1].FontData.y, WHITE);

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    PreviousMenu = MENU_WELCOME_VENDOR_SignUp;
                    NextMenu = MENU_WELCOME_VENDOR_SignUp;
                    CurrentMenu = NextMenu;

                    PlaySound(ButtonClickSFX);
                    SuccessfulWriteUserAccount = false;
                    HasClicked[2] = false;
                    Transitioning = true;
                    ScreenFade = 1.0f;
                }
            } else {
                for (int i = 0; i < 2; i++) {
                    DrawTextEx(GIFont, SignUpMessage[i].TextFill, SignUpMessage[i].TextPosition, SignUpMessage[i].FontData.x, SignUpMessage[i].FontData.y, (i % 2 == 0) ? ORANGE : PINK);
                };

                for (int i = 7; i < 9; i++) {
                    if (CheckCollisionPointRec(Mouse, Buttons[i - 7])) {
                        HoverTransition[i] = CustomLerp(HoverTransition[i], 1.0f, 0.1f);
                        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                            if (i == 7) NextMenu = MENU_WELCOME_VENDOR_SignUp;
                            if (i == 8) {
                                SuccessfulWriteUserAccount = false;
                                UserAccountExists = false;
                                UserAccountIndex = -1;

                                HasClicked[2] = true;
                                
                                for (int CurrentAccountIndex = 0; CurrentAccountIndex < MAX_ACCOUNT_LIMIT; CurrentAccountIndex++) {
                                    if (strcmp(OriginalVendors[CurrentAccountIndex].Username, WelcomeInputList[2].Text) == 0 &&
                                        strcmp(OriginalVendors[CurrentAccountIndex].Password, WelcomeInputList[3].Text) == 0) {
                                        UserAccountExists = true;
                                        break;
                                    }

                                    if (strlen(OriginalVendors[CurrentAccountIndex].Username) == 0 && strlen(OriginalVendors[CurrentAccountIndex].Password) == 0) {
                                        UserAccountIndex = CurrentAccountIndex;
                                        break;
                                    }
                                }

                                if (!UserAccountExists) {
                                    if (UserAccountIndex != -1) {
                                        strcpy(OriginalVendors[UserAccountIndex].Username, WelcomeInputList[2].Text);
                                        strcpy(OriginalVendors[UserAccountIndex].Password, WelcomeInputList[3].Text);
                                        strcpy(OriginalVendors[UserAccountIndex].AffiliatedSchoolData.Name, "");
                                        strcpy(OriginalVendors[UserAccountIndex].AffiliatedSchoolData.Students, "");
                                        strcpy(OriginalVendors[UserAccountIndex].AffiliatedSchoolData.AffiliatedVendor, "");
                                        for (int j = 0; j < 5; j++) {
                                            strcpy(OriginalVendors[UserAccountIndex].Menus[j].Carbohydrate, "");
                                            strcpy(OriginalVendors[UserAccountIndex].Menus[j].Protein, "");
                                            strcpy(OriginalVendors[UserAccountIndex].Menus[j].Vegetables, "");
                                            strcpy(OriginalVendors[UserAccountIndex].Menus[j].Fruits, "");
                                            strcpy(OriginalVendors[UserAccountIndex].Menus[j].Milk, "");
                                            OriginalVendors[UserAccountIndex].BudgetPlanConfirmation[j] = 0;
                                        }
                                        OriginalVendors[UserAccountIndex].SaveSchoolDataIndex = -1;
                                        
                                        WriteUserAccount(OriginalVendors);
                                        SyncVendorsFromOriginalVendorData(Vendors, OriginalVendors);
                                        SuccessfulWriteUserAccount = true;
                                    } else {
                                        // TODO: Handle full database for the users.
                                        // printf("Error: User database full!\n");
                                    }
                                }
                            }
                            
                            PlaySound(ButtonClickSFX);
                            Transitioning = true;
                            ScreenFade = 1.0f;
                        }
                    } else HoverTransition[i] = CustomLerp(HoverTransition[i], 0.0f, 0.1f);
                    
                    if (i == 7) { // Reset button
                        ButtonColor[i - 7] = (HoverTransition[i] > 0.5f) ? RED : LIGHTGRAY;
                        TextColor[i - 7] = (HoverTransition[i] > 0.5f) ? WHITE : BLACK;
                    } else if (i == 8) { // Submit button
                        ButtonColor[i - 7] = (HoverTransition[i] > 0.5f) ? GREEN : LIGHTGRAY;
                        TextColor[i - 7] = (HoverTransition[i] > 0.5f) ? WHITE : BLACK;
                    }

                    float scale = 1.0f + HoverTransition[i] * 0.1f;
                    
                    float newWidth = Buttons[i - 7].width * scale;
                    float newHeight = Buttons[i - 7].height * scale;
                    float newX = Buttons[i - 7].x - (newWidth - Buttons[i - 7].width) / 2;
                    float newY = Buttons[i - 7].y - (newHeight - Buttons[i - 7].height) / 2;
                    
                    DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, ButtonColor[i - 7]);
                    
                    Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[i - 7], (FONT_P * scale), SPACING_WIDER);
                    int textX = newX + (newWidth / 2) - (textWidth.x / 2);
                    int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
                    DrawTextEx(GIFont, ButtonTexts[i - 7], (Vector2){textX, textY}, (FONT_P * scale), SPACING_WIDER, TextColor[i - 7]);
                }

                if (CheckCollisionPointRec(Mouse, Buttons[2])) {
                    HoverTransition[1021] = CustomLerp(HoverTransition[1021], 1.0f, 0.1f);
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        ShowPassword = !ShowPassword;
                        
                        PlaySound(ButtonClickSFX);
                        Transitioning = true;
                        ScreenFade = 1.0f;
                    }
                } else HoverTransition[1021] = CustomLerp(HoverTransition[1021], 0.0f, 0.1f);
                
                if (!ShowPassword) { // Show button
                    ButtonColor[2] = (HoverTransition[1021] > 0.5f) ? SKYBLUE : LIGHTGRAY;
                    TextColor[2] = (HoverTransition[1021] > 0.5f) ? WHITE : BLACK;
                } else { // Hide button
                    ButtonColor[2] = (HoverTransition[1021] > 0.5f) ? GOLD : LIGHTGRAY;
                    TextColor[2] = (HoverTransition[1021] > 0.5f) ? WHITE : BLACK;
                }

                float scale = 1.0f + HoverTransition[1021] * 0.1f;
                
                float newWidth = Buttons[2].width * scale;
                float newHeight = Buttons[2].height * scale;
                float newX = Buttons[2].x - (newWidth - Buttons[2].width) / 2;
                float newY = Buttons[2].y - (newHeight - Buttons[2].height) / 2;
                
                if (ShowPassword) {
                    DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, ButtonColor[2]);
                    
                    Vector2 textWidth = MeasureTextEx(GIFont, ButtonShowPassword[1], (FONT_H3 * scale), SPACING_WIDER);
                    int textX = newX + (newWidth / 2) - (textWidth.x / 2);
                    int textY = newY + (newHeight / 2) - (FONT_H3 * scale / 2);
                    DrawTextEx(GIFont, ButtonShowPassword[1], (Vector2){textX, textY}, (FONT_H3 * scale), SPACING_WIDER, TextColor[2]);
                } else {
                    DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, ButtonColor[2]);
                    
                    Vector2 textWidth = MeasureTextEx(GIFont, ButtonShowPassword[0], (FONT_H3 * scale), SPACING_WIDER);
                    int textX = newX + (newWidth / 2) - (textWidth.x / 2);
                    int textY = newY + (newHeight / 2) - (FONT_H3 * scale / 2);
                    DrawTextEx(GIFont, ButtonShowPassword[0], (Vector2){textX, textY}, (FONT_H3 * scale), SPACING_WIDER, TextColor[2]);
                }

                DrawTextEx(GIFont, DataConfirmationInfo[0].TextFill, DataConfirmationInfo[0].TextPosition, DataConfirmationInfo[0].FontData.x, DataConfirmationInfo[0].FontData.y, WHITE);
                DrawTextEx(GIFont, DataConfirmationInfo[2].TextFill, DataConfirmationInfo[2].TextPosition, DataConfirmationInfo[2].FontData.x, DataConfirmationInfo[2].FontData.y, WHITE);
                
                DrawTextEx(GIFont, DataConfirmationInfo[1].TextFill, DataConfirmationInfo[1].TextPosition, DataConfirmationInfo[1].FontData.x, DataConfirmationInfo[1].FontData.y, WHITE);
                DrawTextEx(GIFont, DataConfirmationInfo[3].TextFill, DataConfirmationInfo[3].TextPosition, DataConfirmationInfo[3].FontData.x, DataConfirmationInfo[3].FontData.y, WHITE);
            }
        }
        
        else if (CurrentMenu == MENU_WELCOME_VENDOR_SignIn) {
            DrawTextEx(GIFont, TitleMessage.TextFill, TitleMessage.TextPosition, TitleMessage.FontData.x, TitleMessage.FontData.y, WHITE);
            DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

            const char *DataTexts[] = {
                "Silakan untuk menyertakan data pribadi Anda sebagai Vendor (atau Catering) terlebih dahulu...",
                "Nama/Kode Vendor (atau Catering)",
                "Kata Sandi",
                
                "Maks.: 30 Karakter (3 u/ Kode)",
                "Maks.: 30 Karakter",
                
                "Mohon maaf, terdapat input data pribadi Anda yang salah. Silakan untuk disertakan ulang kembali...",
                
                "Anda masih menyertakan data yang salah.",
                "Kesempatan terakhir telah diberikan kepada Anda untuk mencoba masuk sebagai Pengguna...",
                
                "Anda sudah TIGA (3) kali gagal masuk sebagai Pengguna.",
                "Anda akan diarahkan kembali ke menu utama dan akses masuk sebagai Pengguna akan",
                "DITUTUP selama beberapa waktu ke depan!"
            };
            const char *ButtonTexts[2] = {"Reset...", "Sign In..."};
            
            Rectangle ResetButton = {(SCREEN_WIDTH / 2) - ResetSubmitButtonSize.x - (ResetSubmitButtonGap / 2), WelcomeInputList[1 + 4].Box.y + ButtonsGap, ResetSubmitButtonSize.x, ResetSubmitButtonSize.y};
            Rectangle SubmitButton = {ResetButton.x + ResetSubmitButtonSize.x + ResetSubmitButtonGap, ResetButton.y, ResetSubmitButtonSize.x, ResetSubmitButtonSize.y};
            Rectangle Buttons[2] = {ResetButton, SubmitButton};
            Color ButtonColor[2] = { 0 }, TextColor[2] = { 0 };

            TextObject InputBoxHelp[2] = {
                CreateTextObject(GIFont, DataTexts[1], (WelcomeInputBoxSize.y / 2), SPACING_WIDER, (Vector2){WelcomeInputList[0 + 4].Box.x + 10, WelcomeInputList[0 + 4].Box.y + 15}, 0, false),
                CreateTextObject(GIFont, DataTexts[2], (WelcomeInputBoxSize.y / 2), SPACING_WIDER, (Vector2){WelcomeInputList[1 + 4].Box.x + 10, WelcomeInputList[1 + 4].Box.y + 15}, 0, false)
            };
            TextObject MaxCharsInfo[2] = {
                CreateTextObject(GIFont, DataTexts[3], (WelcomeInputBoxSize.y / 3), SPACING_WIDER, (Vector2){WelcomeInputList[0 + 4].Box.x + WelcomeInputList[0 + 4].Box.width - (MeasureTextEx(GIFont, DataTexts[3], (WelcomeInputBoxSize.y / 3), SPACING_WIDER).x) - 15, WelcomeInputList[0 + 4].Box.y + (WelcomeInputBoxSize.y / 3)}, 0, false),
                CreateTextObject(GIFont, DataTexts[4], (WelcomeInputBoxSize.y / 3), SPACING_WIDER, (Vector2){WelcomeInputList[1 + 4].Box.x + WelcomeInputList[1 + 4].Box.width - (MeasureTextEx(GIFont, DataTexts[4], (WelcomeInputBoxSize.y / 3), SPACING_WIDER).x) - 15, WelcomeInputList[1 + 4].Box.y + (WelcomeInputBoxSize.y / 3)}, 0, false)
            };
            
            TextObject SignInMessage = CreateTextObject(GIFont, DataTexts[0], FONT_H3, SPACING_WIDER, (Vector2){0, 280}, 0, true);
            TextObject FirstErrorMessage = CreateTextObject(GIFont, DataTexts[5], FONT_H3, SPACING_WIDER, (Vector2){0, SignInMessage.TextPosition.y}, 40, true);
            TextObject SecondErrorMessage[2] = {
                CreateTextObject(GIFont, DataTexts[6], FONT_H3, SPACING_WIDER, (Vector2){0, SignInMessage.TextPosition.y}, 40, true),
                CreateTextObject(GIFont, DataTexts[7], FONT_H3, SPACING_WIDER, (Vector2){0, SecondErrorMessage[0].TextPosition.y}, 40, true),
            };
            TextObject ThirdErrorMessage[3] = {
                CreateTextObject(GIFont, DataTexts[8], FONT_H3, SPACING_WIDER, (Vector2){0, SignInMessage.TextPosition.y}, 0, true),
                CreateTextObject(GIFont, DataTexts[9], FONT_H3, SPACING_WIDER, (Vector2){0, ThirdErrorMessage[0].TextPosition.y}, 40, true),
                CreateTextObject(GIFont, DataTexts[10], FONT_H3, SPACING_WIDER, (Vector2){0, ThirdErrorMessage[1].TextPosition.y}, 40, true),
            };
            
            if (ST_User.SignInChance < 0) {
                for (int i = 0; i < 3; i++) { DrawTextEx(GIFont, ThirdErrorMessage[i].TextFill, ThirdErrorMessage[i].TextPosition, ThirdErrorMessage[i].FontData.x, ThirdErrorMessage[i].FontData.y, RED); }
                CurrentFrameTime += GetFrameTime();

                if (CurrentFrameTime >= 3.0f) {
                    PreviousMenu = MENU_WELCOME_VENDOR;
                    NextMenu = MENU_WELCOME_VENDOR;
                    CurrentMenu = NextMenu;

                    ST_User.AllValid = false; ST_User.LastTime = ST_User.CurrentTime; ST_User.CooldownTime *= 1;
                    UpdateLastSignInTime(&ST_User, "/data/"USER_SAVEFILE);

                    HasClicked[3] = false; CheckForSignInClick = false;
                    Transitioning = true;
                    ScreenFade = 1.0f;
                    
                    CurrentFrameTime = 0;
                    for (int i = 0; i < 2; i++) { WelcomeInputList[i + 4].IsValid = true; }
                }
            } else {
                PreviousMenu = MENU_WELCOME_VENDOR;
                DrawPreviousMenuButton();
                
                DrawTextEx(GIFont, SignInMessage.TextFill, SignInMessage.TextPosition, SignInMessage.FontData.x, SignInMessage.FontData.y, PINK);
                DrawRectangleRec(WelcomeInputList[0 + 4].Box, GRAY);
                DrawRectangleRec(WelcomeInputList[1 + 4].Box, GRAY);

                if (ST_User.SignInChance > 0 && ST_User.SignInChance < 3) {
                    DrawTextEx(GIFont, FirstErrorMessage.TextFill, FirstErrorMessage.TextPosition, FirstErrorMessage.FontData.x, FirstErrorMessage.FontData.y, RED);
                } else if (ST_User.SignInChance == 0) {
                    DrawTextEx(GIFont, FirstErrorMessage.TextFill, FirstErrorMessage.TextPosition, FirstErrorMessage.FontData.x, FirstErrorMessage.FontData.y, Fade(BLACK, 0));
                    for (int i = 0; i < 2; i++) { DrawTextEx(GIFont, SecondErrorMessage[i].TextFill, SecondErrorMessage[i].TextPosition, SecondErrorMessage[i].FontData.x, SecondErrorMessage[i].FontData.y, RED); }
                }

                for (int i = 9; i < 11; i++) {
                    if (CheckCollisionPointRec(Mouse, Buttons[i - 9])) {
                        HoverTransition[i] = CustomLerp(HoverTransition[i], 1.0f, 0.1f);
                        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                            if (i == 9) ResetInputs(WelcomeInputList, (Vector2){4, 6});
                            if (i == 10) {
                                CheckForSignInClick = true;
                                ST_User.AllValid = true;

                                for (int j = 0; j < 2; j++) {
                                    WelcomeInputList[j + 4].IsValid = (strlen(WelcomeInputList[j + 4].Text) >= 3);
                                    if (!WelcomeInputList[j + 4].IsValid) ST_User.AllValid = false;
                                } HasClicked[3] = true;
                            }
                            
                            PlaySound(ButtonClickSFX);
                            Transitioning = true;
                            ScreenFade = 1.0f;
                        }
                    } else HoverTransition[i] = CustomLerp(HoverTransition[i], 0.0f, 0.1f);
                    
                    if (i == 9) { // Reset button
                        ButtonColor[i] = (HoverTransition[i] > 0.5f) ? RED : LIGHTGRAY;
                        TextColor[i] = (HoverTransition[i] > 0.5f) ? WHITE : BLACK;
                    } else if (i == 10) { // Submit button
                        ButtonColor[i] = (HoverTransition[i] > 0.5f) ? GREEN : LIGHTGRAY;
                        TextColor[i] = (HoverTransition[i] > 0.5f) ? WHITE : BLACK;
                    }

                    float scale = 1.0f + HoverTransition[i] * 0.1f;
                    
                    float newWidth = Buttons[i - 9].width * scale;
                    float newHeight = Buttons[i - 9].height * scale;
                    float newX = Buttons[i - 9].x - (newWidth - Buttons[i - 9].width) / 2;
                    float newY = Buttons[i - 9].y - (newHeight - Buttons[i - 9].height) / 2;
                    
                    DrawRectangleRec((Rectangle){newX, newY, newWidth, newHeight}, ButtonColor[i]);
                    
                    Vector2 textWidth = MeasureTextEx(GIFont, ButtonTexts[i - 9], (FONT_P * scale), SPACING_WIDER);
                    int textX = newX + (newWidth / 2) - (textWidth.x / 2);
                    int textY = newY + (newHeight / 2) - (FONT_P * scale / 2);
                    DrawTextEx(GIFont, ButtonTexts[i - 9], (Vector2){textX, textY}, (FONT_P * scale), SPACING_WIDER, TextColor[i]);
                }
                
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    ActiveBox = -1;  // Reset active box
                    for (int i = 0; i < 2; i++) {
                        if (CheckCollisionPointRec(Mouse, WelcomeInputList[i + 4].Box)) {
                            ActiveBox = i + 4;
                            break;
                        }
                    }
                }

                if (ActiveBox != -1) {
                    int Key = GetCharPressed();
                    while (Key > 0) {
                        if (strlen(WelcomeInputList[ActiveBox].Text) < MAX_INPUT_LENGTH) {
                            int len = strlen(WelcomeInputList[ActiveBox].Text);
                            WelcomeInputList[ActiveBox].Text[len] = (char)Key;
                            WelcomeInputList[ActiveBox].Text[len + 1] = '\0';
                            if (ActiveBox % 2 != 0) {
                                WelcomeInputList[ActiveBox].MaskedPassword[len - 1] = '*';
                                WelcomeInputList[ActiveBox].MaskedPassword[len] = WelcomeInputList[ActiveBox].Text[len];
                                WelcomeInputList[ActiveBox].MaskedPassword[len + 1] = '\0';
                            }
                        } Key = GetCharPressed();
                    }

                    OccuringLastCharacter += GetFrameTime();
                    if (OccuringLastCharacter >= 1.0f) {
                        WelcomeInputList[ActiveBox].MaskedPassword[strlen(WelcomeInputList[ActiveBox].MaskedPassword) - 1] = '*';
                        OccuringLastCharacter = 0;
                    }

                    if (IsKeyPressed(KEY_BACKSPACE) && strlen(WelcomeInputList[ActiveBox].Text) > 0) {
                        WelcomeInputList[ActiveBox].Text[strlen(WelcomeInputList[ActiveBox].Text) - 1] = '\0';
                        if (ActiveBox % 2 != 0) WelcomeInputList[ActiveBox].MaskedPassword[strlen(WelcomeInputList[ActiveBox].MaskedPassword) - 1] = '\0';
                    } else if (IsKeyDown(KEY_BACKSPACE) && strlen(WelcomeInputList[ActiveBox].Text) > 0) {
                        CurrentFrameTime += GetFrameTime();
                        if (CurrentFrameTime >= 0.5f) {
                            WelcomeInputList[ActiveBox].Text[strlen(WelcomeInputList[ActiveBox].Text) - 1] = '\0';
                            if (ActiveBox % 2 != 0) WelcomeInputList[ActiveBox].MaskedPassword[strlen(WelcomeInputList[ActiveBox].MaskedPassword) - 1] = '\0';
                        }
                    }
                    
                    if (IsKeyReleased(KEY_BACKSPACE)) CurrentFrameTime = 0;
                }

                FrameCounter++; // Increment frame counter for cursor blinking
                for (int i = 0; i < 2; i++) {
                    Color boxColor = GRAY;
                    if (!WelcomeInputList[i + 4].IsValid && HasClicked[3]) { boxColor = RED; }
                    
                    if ((i + 4) == ActiveBox) boxColor = SKYBLUE; // Active input
                    else if (CheckCollisionPointRec(Mouse, WelcomeInputList[i + 4].Box)) boxColor = YELLOW; // Hover effect

                    DrawRectangleRec(WelcomeInputList[i + 4].Box, boxColor);
                    if (i % 2 == 0) DrawTextEx(GIFont, WelcomeInputList[i + 4].Text, (Vector2){WelcomeInputList[i + 4].Box.x + 10, WelcomeInputList[i + 4].Box.y + 15}, (WelcomeInputBoxSize.y / 2), SPACING_WIDER, BLACK);
                    else DrawTextEx(GIFont, WelcomeInputList[i + 4].MaskedPassword, (Vector2){WelcomeInputList[i + 4].Box.x + 10, WelcomeInputList[i + 4].Box.y + 15}, (WelcomeInputBoxSize.y / 2), SPACING_WIDER, BLACK);

                    if ((i + 4) == ActiveBox && (FrameCounter / CURSOR_BLINK_SPEED) % 2 == 0) {
                        int textWidth = MeasureTextEx(GIFont, WelcomeInputList[i + 4].MaskedPassword, (WelcomeInputBoxSize.y / 2), SPACING_WIDER).x;
                        if (ActiveBox % 2 == 0) textWidth = MeasureTextEx(GIFont, WelcomeInputList[i + 4].Text, (WelcomeInputBoxSize.y / 2), SPACING_WIDER).x;
                        else textWidth = MeasureTextEx(GIFont, WelcomeInputList[i + 4].MaskedPassword, (WelcomeInputBoxSize.y / 2), SPACING_WIDER).x;
                        DrawTextEx(GIFont, "|", (Vector2){WelcomeInputList[i + 4].Box.x + 10 + textWidth, WelcomeInputList[i + 4].Box.y + 10}, (WelcomeInputBoxSize.y / 2) + 10, SPACING_WIDER, BLACK);
                    }
                    
                    DrawTextEx(GIFont, MaxCharsInfo[i].TextFill, MaxCharsInfo[i].TextPosition, MaxCharsInfo[i].FontData.x, MaxCharsInfo[i].FontData.y, BLACK);
                    if (strlen(WelcomeInputList[i + 4].Text) == 0 && (i + 4) != ActiveBox) {
                        DrawTextEx(GIFont, InputBoxHelp[i].TextFill, InputBoxHelp[i].TextPosition, InputBoxHelp[i].FontData.x, InputBoxHelp[i].FontData.y, Fade(BLACK, 0.6));
                    }
                }

                if (HasClicked[3]) {
                    if (ST_User.AllValid) {
                        ST_User.AllValid = false;

                        ReadVendors = ReadUserAccount(OriginalVendors);
                        for (int UserAccountIndex = 0; UserAccountIndex < ReadVendors; UserAccountIndex++) {
                            if (strcmp(OriginalVendors[UserAccountIndex].Username, WelcomeInputList[4].Text) == 0 && strcmp(OriginalVendors[UserAccountIndex].Password, WelcomeInputList[5].Text) == 0) {
                                User.Vendor = OriginalVendors[UserAccountIndex];
                                User.VendorID = UserAccountIndex;
                                
                                ST_User.CooldownTime = 15;
                                ST_User.LastTime = ST_User.CurrentTime;
                                UpdateLastSignInTime(&ST_User, "/data/"USER_SAVEFILE);
                                
                                PreviousMenu = CurrentMenu;
                                NextMenu = MENU_WELCOME_VENDOR_MainMenuTransition;
                                CurrentMenu = NextMenu;

                                HasClicked[3] = false;
                                Transitioning = true;
                                ScreenFade = 1.0f;

                                break;
                            }
                        }
                    } else {
                        if (CheckForSignInClick) {
                            ST_User.SignInChance--;
                            ST_User.LastTime = ST_User.CurrentTime;
                            UpdateLastSignInTime(&ST_User, "/data/"USER_SAVEFILE);
                            
                            CheckForSignInClick = false;
                            HasClicked[3] = false;
                        }
                    }
                }
            }
        } else if (CurrentMenu == MENU_WELCOME_VENDOR_MainMenuTransition) {
            DrawTextEx(GIFont, TitleMessage.TextFill, TitleMessage.TextPosition, TitleMessage.FontData.x, TitleMessage.FontData.y, WHITE);
            DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);
            
            char UserSuccessNotification[256] = { 0 };
            char *SuccessMessage[2] = {
                "Anda telah berhasil masuk sebagai Pengguna, bertugas sebagai Vendor (atau Catering).",
                "Sesaat setelah ini, Anda akan segera diarahkan ke menu utama Pengguna. Selamat bekerja," // "%s!"
            };
            snprintf(UserSuccessNotification, sizeof(UserSuccessNotification), "%s %s!", SuccessMessage[1], User.Vendor.Username);
            
            TextObject FirstInfoMessage = CreateTextObject(GIFont, SuccessMessage[0], FONT_H2, SPACING_WIDER, (Vector2){0, 520}, 0, true);
            TextObject SecondInfoMessage = CreateTextObject(GIFont, UserSuccessNotification, FONT_H3, SPACING_WIDER, (Vector2){0, FirstInfoMessage.TextPosition.y}, 60, true);

            DrawTextEx(GIFont, FirstInfoMessage.TextFill, FirstInfoMessage.TextPosition, FirstInfoMessage.FontData.x, FirstInfoMessage.FontData.y, GREEN);
            DrawTextEx(GIFont, SecondInfoMessage.TextFill, SecondInfoMessage.TextPosition, SecondInfoMessage.FontData.x, SecondInfoMessage.FontData.y, SKYBLUE);

            CurrentFrameTime += GetFrameTime();
            if (CurrentFrameTime >= 3.0f) {
                PreviousMenu = CurrentMenu;
                NextMenu = MENU_MAIN_VENDOR;
                CurrentMenu = NextMenu;

                Transitioning = true;
                ScreenFade = 1.0f;
                CurrentFrameTime = 0;
            }
            
            ST_User.SignInChance = 3;
            UpdateLastSignInTime(&ST_User, "/data/"USER_SAVEFILE);
        }

        else if (CurrentMenu == MENU_MAIN_GOVERNMENT) GovernmentMainMenu();
        else if (CurrentMenu == MENU_MAIN_GOVERNMENT_SchoolManagement) {
            GovernmentManageSchoolsMenu();
        } else if (CurrentMenu == MENU_MAIN_GOVERNMENT_SchoolManagement_ADD) {
            GovernmentManageSchoolsMenu_ADD();
        } else if (CurrentMenu == MENU_MAIN_GOVERNMENT_SchoolManagement_EDIT) {
            GovernmentManageSchoolsMenu_EDIT();
        } else if (CurrentMenu == MENU_MAIN_GOVERNMENT_SchoolManagement_DELETE) {
            GovernmentManageSchoolsMenu_DELETE();
        }
        else if (CurrentMenu == MENU_MAIN_GOVERNMENT_BilateralManagement) {
            GovernmentManageBilateralsMenu();
        } else if (CurrentMenu == MENU_MAIN_GOVERNMENT_BilateralManagement_CHOOSING) {
            GovernmentManageBilateralsMenu_CHOOSING();
        }

        else if (CurrentMenu == MENU_MAIN_GOVERNMENT_GetVendorList) {
            GovernmentViewVendorsMenu();
        } else if (CurrentMenu == MENU_MAIN_GOVERNMENT_GetVendorList_DETAILS) {
            GovernmentViewVendorsMenu_DETAILS();
        }
        else if (CurrentMenu == MENU_MAIN_GOVERNMENT_ConfirmBudgetPlan) {
            GovernmentConfirmBudgetPlansMenu();
        } else if (CurrentMenu == MENU_MAIN_GOVERNMENT_ConfirmBudgetPlan_CHOOSING) {
            GovernmentConfirmBudgetPlansMenu_CHOOSING();
        }
        else if (CurrentMenu == MENU_MAIN_GOVERNMENT_GetBilateralList) GovernmentViewBilateralsMenu();

        else if (CurrentMenu == MENU_MAIN_VENDOR) VendorMainMenu();
        else if (CurrentMenu == MENU_MAIN_VENDOR_MenusManagement) {
            VendorManageDailyMenusMenu();
        } else if (CurrentMenu == MENU_MAIN_VENDOR_MenusManagement_ADD) {
            VendorManageDailyMenusMenu_ADD();
        } else if (CurrentMenu == MENU_MAIN_VENDOR_MenusManagement_EDIT) {
            VendorManageDailyMenusMenu_EDIT();
        } else if (CurrentMenu == MENU_MAIN_VENDOR_MenusManagement_DELETE) {
            VendorManageDailyMenusMenu_DELETE();
        }
        else if (CurrentMenu == MENU_MAIN_VENDOR_GetAffiliatedSchoolData) {
            VendorGetAffiliatedSchoolData();
        }
        else if (CurrentMenu == MENU_MAIN_VENDOR_SubmitBudgetPlan) {
            VendorRequestBudgetPlanMenu();
        }
        else if (CurrentMenu == MENU_MAIN_VENDOR_GetDailyMenuStatusList) {
            VendorGetDailyMenuStatusList();
        }
        
        else if (CurrentMenu == MENU_ABOUT) {
            DrawTextEx(GIFont, TitleMessage.TextFill, TitleMessage.TextPosition, TitleMessage.FontData.x, TitleMessage.FontData.y, WHITE);
            DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

            DrawPreviousMenuButton();

            const char *AboutMessages[] = {
                "Aplikasi D'Magis dirancang untuk memfasilitasi kerja sama antara pemerintah dan penyedia layanan\nkatering (Vendor) dalam penyediaan makanan sekolah. Dengan sistem yang terstruktur, aplikasi ini\nmemungkinkan pengelolaan data sekolah, penetapan kerja sama, serta proses pengajuan dan persetujuan\nRencana Anggaran Biaya (RAB) untuk program makanan sehat bagi siswa.\n\nProgram ini dibuat sebagai bentuk visualisasi dan hasil rekreasi dari Tugas Besar yang diberikan\noleh Mata Praktikum: Algoritma dan Dasar Pemrograman, yang berada di bawah naungan Laboratorium\nDasar Komputer. Proyek Tugas Besar ini dikerjakan oleh Divisi Assignment and Task Committe 1,\ndan dikhususkan untuk para praktikan di Semester 2 ini.\n\nKontributor:\n1. NUE (Immanuel E. H. Joseph A.): Programmer\n2. RAF (Rafhan Mazaya F.): Founder, Flowchart Maker (Admin as Government)\n3. SNI (Stevannie Pratama): Flowchart Maker (User as Vendor/Catering)\n4. DAZ (Dariele Zebada S. G.): Flowchart Maker (Profile Credentials)",
                
                "Project Link: github.com/EintsWaveX/DMagis",
                "Project Creation Timeline: 31st January 2025 u/ 10th April 2025 (maintenanced until today)",
                "Project Publishment: Thursday, April 10th 2025, at 3 AM",

                "Copyrighted (C) 2025, authorized by ATC 1, under Basic Computing Laboratory guidance. All rights reserved.",
            };
            TextObject AM[5] ={
                CreateTextObject(GIFont, AboutMessages[0], FONT_P, SPACING_WIDER, (Vector2){0, 260}, 0, true),
                CreateTextObject(GIFont, AboutMessages[1], FONT_P, SPACING_WIDER, AM[0].TextPosition, 555, true),
                CreateTextObject(GIFont, AboutMessages[2], FONT_P, SPACING_WIDER, AM[1].TextPosition, 30, true),
                CreateTextObject(GIFont, AboutMessages[3], FONT_P, SPACING_WIDER, AM[2].TextPosition, 30, true),
                CreateTextObject(GIFont, AboutMessages[4], FONT_P, SPACING_WIDER, (Vector2){0, SCREEN_HEIGHT - 60}, 0, true),
            };

            DrawTextEx(GIFont, AM[0].TextFill, AM[0].TextPosition, AM[0].FontData.x, AM[0].FontData.y, WHITE);
            DrawTextEx(GIFont, AM[1].TextFill, AM[1].TextPosition, AM[1].FontData.x, AM[1].FontData.y, SKYBLUE);
            DrawTextEx(GIFont, AM[2].TextFill, AM[2].TextPosition, AM[2].FontData.x, AM[2].FontData.y, YELLOW);
            DrawTextEx(GIFont, AM[3].TextFill, AM[3].TextPosition, AM[3].FontData.x, AM[3].FontData.y, PINK);
            DrawTextEx(GIFont, AM[4].TextFill, AM[4].TextPosition, AM[4].FontData.x, AM[4].FontData.y, GREEN);
        }
        else if (CurrentMenu == MENU_EXIT) {
            DrawTextEx(GIFont, TitleMessage.TextFill, TitleMessage.TextPosition, TitleMessage.FontData.x, TitleMessage.FontData.y, WHITE);
            DrawTextEx(GIFont, SubtitleMessage.TextFill, SubtitleMessage.TextPosition, SubtitleMessage.FontData.x, SubtitleMessage.FontData.y, WHITE);

            const char *ExitingMessage[] = {
                "Terima kasih telah menggunakan jasa aplikasi D'Magis!",
                "Tekan di mana saja untuk segera keluar dari aplikasi... Sampai bertemu kembali!"
            };
            TextObject EM1 = CreateTextObject(GIFont, ExitingMessage[0], FONT_P, SPACING_WIDER, (Vector2){0, 520}, 0, true);
            TextObject EM2 = CreateTextObject(GIFont, ExitingMessage[1], FONT_P, SPACING_WIDER, (Vector2){0, EM1.TextPosition.y}, 40, true);
           
            DrawTextEx(GIFont, EM1.TextFill, EM1.TextPosition, EM1.FontData.x, EM1.FontData.y, WHITE);
            DrawTextEx(GIFont, EM2.TextFill, EM2.TextPosition, EM2.FontData.x, EM2.FontData.y, WHITE);

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) break;
        }

        else if (CurrentMenu == MENU_SUBMAIN_SortingOptions) {
            MenuSubMain_SortingOptions();
        }

        if (Transitioning) DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(GetColor(0x181818), ScreenFade));
        EndDrawing();
    }

    for (int i = 0; i < MAX_BGM; i++) { UnloadMusicStream(BackgroundMusics[i]); }
    UnloadSound(ButtonClickSFX);
    UnloadFont(GIFont);
    UnloadShader(shader);

    CloseAudioDevice();
    CloseWindow();

    return 0;
}

#endif

// Small fix to trigger the changes and updates on the deployment.
// Always settle it this way.