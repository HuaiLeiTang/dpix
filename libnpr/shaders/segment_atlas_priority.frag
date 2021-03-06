/*****************************************************************************\

segment_atlas_priority.frag
Author: Forrester Cole (fcole@cs.princeton.edu)
Copyright (c) 2009 Forrester Cole

Rendering priority values into the segment atlas.

libnpr is distributed under the terms of the GNU General Public License.
See the COPYING file for details.

\*****************************************************************************/


uniform vec4 viewport;

uniform sampler2DRect priority_buffer;

varying vec4 sample_clip_pos;
varying float path_id;

float getPriorityValue(vec3 id, vec3 position)
{
    float count = 0.0;
    float comparison_width = 1000.0;
    for (float i = -2.0; i <= 2.0; i++)
    {
        for (float j = -2.0; j <= 2.0; j++)
        {
            vec2 offset = vec2(i, j);
            vec4 buffer_texel = texture2DRect(priority_buffer, 
                                              position.xy + offset);
            vec3 buffer_color = buffer_texel.rgb;
            comparison_width = min(comparison_width, buffer_texel.a*255.0);

            if (idsEqual(buffer_color, id)) 
                count = count + 1.0;
        }
    }
    comparison_width = max(comparison_width, 1.0);
    return max(min(count / (min(comparison_width, 5.0)), 1.0), 0.0);
}

void main()
{
    vec3 path_color = idToColor(path_id);

    // convert from clip to window coordinates
    vec3 sample_window_pos;
    vec3 sample_post_div = sample_clip_pos.xyz / sample_clip_pos.w;

    sample_window_pos.xy = (sample_post_div.xy + vec2(1.0,1.0)) * 0.5 * viewport.zw;
    sample_window_pos.z = 0.5 * (sample_post_div.z + 1.0);

    float priority = getPriorityValue(path_color, sample_window_pos);

    gl_FragData[0] = vec4(path_color, priority);
}
