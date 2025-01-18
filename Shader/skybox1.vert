#version 330 core
layout (location = 0) in vec3 position;

out vec3 TexCoords;

uniform mat4 modelView;     //ModelView matrix
uniform mat4 projection;    //Projection matrix
uniform vec3 cameraPosition;

void main()
{	
	TexCoords = position - cameraPosition;
	
	vec3 pos = position;
	vec4 viewPos = modelView * vec4(pos, 1.0);
    gl_Position = projection * modelView * vec4(pos, 1.0);
}  
