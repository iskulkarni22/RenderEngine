#version 330 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec2 txc;

uniform mat4 mvp;
uniform mat4 mv;

out vec3 vNrm;
out vec3 view;
out vec2 texCoord;


void main()
{
    mat3 camInv = transpose(inverse(mat3(mv)));

    vNrm = normalize(camInv * aNrm);
    view = -vec3(mv * vec4(pos, 1));

    texCoord = txc;
    gl_Position = mvp * vec4(pos, 1);
}