out vec3 normal;

void main() {

	gl_TexCoord[0] = gl_MultiTexCoord0;
	gl_Position = gl_Vertex;
    normal = gl_Normal;
}
