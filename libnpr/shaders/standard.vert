uniform vec3 view_pos;
uniform vec3 light_dir;

varying vec3 vert_pos_world;
varying vec3 vert_pos_camera;
varying vec4 vert_pos_clip;
varying vec3 vert_vec_view;
varying vec3 vert_vec_light;
varying vec3 vert_normal_camera;
varying vec3 vert_normal_world;

//cutaway
varying vec4 cutawayTexCoordH;

void main()
{
    gl_Position = ftransform();
    gl_FrontColor = gl_BackColor = gl_Color;
    gl_TexCoord[0] = gl_MultiTexCoord0;

#if 0
    vec4 worlddir = vec4(gl_Color.xyz, 0.0) * 2.0 - vec4(1.0,1.0,1.0,0.0);
    vec4 projdirw = gl_ModelViewProjectionMatrix * (worlddir + gl_Vertex);
    vec4 projposw = gl_ModelViewProjectionMatrix * gl_Vertex;
    vec3 projdir = (projdirw.xyz / projdirw.w) - (projposw.xyz / projposw.w);
    projdir[2] = 0.0;
    projdir = normalize(projdir);
    float theta = atan(projdir[1],projdir[0]);
    gl_FrontColor = gl_BackColor = colorWheel(theta);
#endif
	
    vert_pos_clip = gl_Position;
    vert_pos_world = gl_Vertex.xyz;
    vert_pos_camera = (gl_ModelViewMatrix * gl_Vertex).xyz;
	
    vert_vec_view = normalize(-vert_pos_camera);
    vert_vec_light = light_dir;
	
    vert_normal_world = normalize(gl_Normal);
    vert_normal_camera = (gl_ModelViewMatrixInverseTranspose * vec4(vert_normal_world, 0)).xyz;
	
    // hack to handle backfacing polygons in bad models
    /*float dotcamerasign = sign(dot(vert_normal_camera, vert_vec_view));
    vert_normal_camera *= dotcamerasign;
    vert_normal_world_ff = vert_normal_world * dotcamerasign;*/

    // get viewport coordinates
    cutawayTexCoordH = gl_ModelViewProjectionMatrix * gl_Vertex;
    // remap to 0..1 space (after perspective divide)
    cutawayTexCoordH.xyw += cutawayTexCoordH.w;
    cutawayTexCoordH.z = (gl_ModelViewMatrix * gl_Vertex).z;
}