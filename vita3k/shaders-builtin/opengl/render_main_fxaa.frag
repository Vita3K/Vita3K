// Vita3K emulator project
// Code adapted from http://horde3d.org/wiki/index.php?title=Shading_Technique_-_FXAA

uniform sampler2D fb;
uniform vec2 inv_frame_size;
in vec2 uv_frag;

out vec3 color_frag;

void main( void ) {
        //gl_FragColor.xyz = texture(buf0,texCoords).xyz;
        //return;

        float FXAA_SPAN_MAX = 8.0;
        float FXAA_REDUCE_MUL = 1.0/8.0;
        float FXAA_REDUCE_MIN = 1.0/128.0;

        vec3 rgbNW=texture(fb,uv_frag+(vec2(-1.0,-1.0)*inv_frame_size)).xyz;
        vec3 rgbNE=texture(fb,uv_frag+(vec2(1.0,-1.0)*inv_frame_size)).xyz;
        vec3 rgbSW=texture(fb,uv_frag+(vec2(-1.0,1.0)*inv_frame_size)).xyz;
        vec3 rgbSE=texture(fb,uv_frag+(vec2(1.0,1.0)*inv_frame_size)).xyz;
        vec3 rgbM=texture(fb,uv_frag).xyz;
        
        vec3 luma=vec3(0.299, 0.587, 0.114);
        float lumaNW = dot(rgbNW, luma);
        float lumaNE = dot(rgbNE, luma);
        float lumaSW = dot(rgbSW, luma);
        float lumaSE = dot(rgbSE, luma);
        float lumaM  = dot(rgbM,  luma);
        
        float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
        float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
        
        vec2 dir;
        dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
        dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
        
        float dirReduce = max(
                (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),
                FXAA_REDUCE_MIN);
          
        float rcpDirMin = 1.0/(min(abs(dir.x), abs(dir.y)) + dirReduce);
        
        dir = min(vec2( FXAA_SPAN_MAX,  FXAA_SPAN_MAX),
                  max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
                  dir * rcpDirMin)) * inv_frame_size;
                
        vec3 rgbA = (1.0/2.0) * (
                texture(fb, uv_frag.xy + dir * (1.0/3.0 - 0.5)).xyz +
                texture(fb, uv_frag.xy + dir * (2.0/3.0 - 0.5)).xyz);
        vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (
                texture(fb, uv_frag.xy + dir * (0.0/3.0 - 0.5)).xyz +
                texture(fb, uv_frag.xy + dir * (3.0/3.0 - 0.5)).xyz);
        float lumaB = dot(rgbB, luma);

        if((lumaB < lumaMin) || (lumaB > lumaMax)){
                color_frag.xyz=rgbA;
        }else{
                color_frag.xyz=rgbB;
        }
}
