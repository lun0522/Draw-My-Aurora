#version 330 core

layout (location = 0) in vec3 aPos;

layout (std140) uniform Matrices {
    uniform mat4 earthModel;
    uniform mat4 viewProjection;
};

void main() {
    gl_Position = viewProjection * earthModel * vec4(aPos, 1.0);
}
