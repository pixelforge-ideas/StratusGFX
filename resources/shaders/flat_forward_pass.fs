STRATUS_GLSL_VERSION

smooth in vec2 fsTexCoords;

//uniform vec3 diffuseColor;
uniform sampler2D diffuseTexture;
uniform vec3 diffuseColor;
uniform bool textured = false;
//uniform float gamma = 2.2;

out vec3 color;

void main() {
    vec3 diffuse = diffuseColor;
    if (textured) {
        diffuse = texture(diffuseTexture, fsTexCoords).xyz;
    }
    // Apply gamma correction
    //texColor = pow(texColor, vec3(1.0 / gamma));
    color = diffuse;
}