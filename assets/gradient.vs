attribute vec2 vertexPosition;
attribute vec4 vertexColor;

varying vec4 vertexColorOut;
uniform mat4 mvp;

void main() {
    vec4 pos = vec4(vertexPosition, 0.0, 1.0);
    gl_Position = mvp * pos;
    vertexColorOut = vertexColor;
}
