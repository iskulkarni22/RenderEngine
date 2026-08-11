#version 330 core
in vec3 vNrm;
in vec3 view;
in vec2 texCoord;

uniform vec3 lightDir;

uniform sampler2D diffTex;
uniform sampler2D specTex;
uniform bool hasDiffTex;
uniform bool hasSpecTex;

uniform vec3 ka;
uniform vec3 kd;
uniform vec3 ks;
uniform float ns;

layout(location = 0) out vec4 color;

void main()
{
    // static color
    vec3 vertexColor = vec3(1.0, 0.8, 1.0);

    // normal buffer shading
    vec3 normal = normalize(vNrm); // interpolated normals need to be normalized
    // vertexColor = normal;

    // blinn shading
    vec3 lightIntensity = 3 * vec3(1.0, 1.0, 1.0);
    vec3 ambientIntensity = vec3(1.0, 1.0, 1.0);
    
    vec3 K_d = kd;
    vec3 K_a = ka;
    if (hasDiffTex) {
        K_d *= texture(diffTex, texCoord).rgb;
    }
    vec3 diffuse = K_d * max(0, dot(normal, lightDir));
    vec3 ambient = ambientIntensity * K_a;

    vec3 K_s = ks;
    if (hasSpecTex) {
        K_s *= texture(specTex, texCoord).rgb;
    }
    
    float alpha = ns;
    vec3 h = normalize(lightDir + normalize(view));

    vec3 specular = K_s * pow(float(max(0, dot(normal, h))), alpha);

    vertexColor = lightIntensity * (diffuse + specular) + ambient;
    color = vec4(vertexColor, 1.0);
}