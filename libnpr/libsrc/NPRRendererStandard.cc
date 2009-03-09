#include "NPRFixedPathSet.h"
#include "NPRDrawable.h"
#include "NPRRendererStandard.h"
#include "NPRSettings.h"
#include "NPRScene.h"
#include "NPRStyle.h"
#include "NPRGLDraw.h"
#include "NPRLight.h"

#include "GQShaderManager.h"
#include "GQStats.h"
#include <assert.h>

NPRRendererStandard::NPRRendererStandard()
{
    clear();
}

void NPRRendererStandard::clear()
{
    _segment_atlas.clear();
}

void NPRRendererStandard::setLineVisibilityMethod( NPRLineVisibilityMethod method )
{
    _line_viz_method = method;
}

void NPRRendererStandard::drawScene( const NPRScene& scene )
{
    __TIME_CODE_BLOCK("Draw Scene");

    NPRSettings& settings = NPRSettings::instance();

    bool skip_stylized_lines = !settings.get(NPR_ENABLE_LINES) || 
                               !settings.get(NPR_ENABLE_STYLIZED_LINES);
    bool skip_lines = !settings.get(NPR_ENABLE_LINES);
    bool skip_polygons = !settings.get(NPR_ENABLE_POLYGONS);

    int viz_method = settings.get(NPR_LINE_VISIBILITY_METHOD);
    setLineVisibilityMethod((NPRLineVisibilityMethod)viz_method);

    if (GQShaderManager::status() == SHADERS_NOT_LOADED)
    {
        GQShaderManager::initialize();
    }

    if (!GQShaderManager::status() == SHADERS_OK)
    {
        NPRGLDraw::clearGLState();
        return;
    }

    if (!skip_stylized_lines)
    {
        __TIME_CODE_BLOCK("Compute Line Vis.");
        drawDepthBuffer(scene);
        
        if (_line_viz_method == NPR_SEGMENT_ATLAS)
        {
            if (settings.get(NPR_VIEW_CLIP_BUFFER))
            {
                _segment_atlas.visualize( scene, *depthBuffer() );
                NPRGLDraw::clearGLState();
                return;
            }

            _segment_atlas.draw( scene, *depthBuffer() );

            if (settings.get(NPR_FILTER_LINE_VISIBILITY))
            {
                _segment_atlas.filter(NPRSegmentAtlas::VISIBILITY_ID, 
                    NPRSegmentAtlas::SMOOTH_AND_THRESHOLD_FILTER,
                    scene.globalStyle());
            }
       
            if (settings.get(NPR_CHECK_LINE_PRIORITY))
            {
                drawPriorityBuffer(scene);

                if (settings.get(NPR_VIEW_PRIORITY_BUFFER))
                {
                    visualizePriorityBuffer();
                    NPRGLDraw::clearGLState();
                    return;
                }

                _segment_atlas.checkPriority(*priorityBuffer());
                if (settings.get(NPR_FILTER_LINE_PRIORITY))
                {
                    _segment_atlas.filter(NPRSegmentAtlas::PRIORITY_ID,
                                          NPRSegmentAtlas::BILATERAL_FILTER, 
                                          0);
                }
                
            }
        }
    }

    NPRGLDraw::clearGLState();
    NPRGLDraw::clearGLScreen(scene.globalStyle()->backgroundColor(), 1);
    if (settings.get(NPR_ENABLE_BACKGROUND_TEXTURE))
    {
        NPRGLDraw::drawBackgroundTex( scene.globalStyle() );
    }

    if (!skip_polygons) {
        __TIME_CODE_BLOCK("Draw Polygons");
        drawPolygons(scene);
    }

    if (skip_stylized_lines && !skip_lines)
    {
        __TIME_CODE_BLOCK("Draw Simple Lines");
        if (skip_polygons)
        {
            __TIME_CODE_BLOCK("Draw Depth Buffer");
            drawSceneDepth(scene);
        }
        _path_renderer.drawSimpleLines(scene);
    }

    if (!skip_stylized_lines) {
        if (_line_viz_method == NPR_SPINE_TEST)
        {
            _path_renderer.drawStrokes(scene, *depthBuffer());
        }
        else if (_line_viz_method == NPR_SEGMENT_ATLAS)
        {
            _path_renderer.drawStrokes(scene, _segment_atlas);
        }
    }

    if (settings.get(NPR_ENABLE_PAPER_TEXTURE))
    {
        NPRGLDraw::clearGLState();
        NPRGLDraw::drawPaperQuad( scene.globalStyle() );
    }

    NPRGLDraw::clearGLState();
}

void NPRRendererStandard::drawSceneDepth( const NPRScene& scene )
{
    NPRGLDraw::clearGLState();

    glDepthMask(GL_TRUE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthFunc(GL_LESS);
    glEnable(GL_DEPTH_TEST);

    glClearDepth(1);
    glClear( GL_DEPTH_BUFFER_BIT);
    
    NPRGLDraw::drawMeshesDepth( scene );

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void NPRRendererStandard::resize(int width, int height)
{
    Q_UNUSED(width);
    Q_UNUSED(height);
}

void NPRRendererStandard::drawPolygons(const NPRScene& scene)
{
    NPRGLDraw::clearGLState();
    NPRGLDraw::clearGLDepth(1);

    glEnable(GL_MULTISAMPLE);
    if (NPRSettings::instance().get(NPR_ENABLE_LIGHTING))
        glEnable(GL_LIGHTING);
    glDisable(GL_BLEND);

    GQShaderRef shader = GQShaderManager::bindProgram("polygon_focus");
    setUniformPolygonParams( shader, scene );

    int mode = NPR_DRAW_POLYGONS | NPR_OPAQUE;
    if (NPRSettings::instance().get(NPR_ENABLE_TRANSPARENT_POLYGONS))
        mode |= NPR_TRANSLUCENT;

    NPRGLDraw::drawMeshes( &shader, scene, 0, mode );
}

void NPRRendererStandard::setUniformPolygonParams(const GQShaderRef& shader, 
                                                  const NPRScene& scene ) 
{
    vec camera_poa = scene.cameraInverseTransform() * scene.focalPoint();
    shader.setUniform3fv("view_pos", scene.cameraPosition() );
    shader.setUniform3fv("light_dir", scene.light(0)->lightDir() );

    const NPRStyle* style = scene.globalStyle();
        
    float transfer[4];

    style->transfer(NPRStyle::COLOR_DESAT).toArray(transfer);
    shader.setUniform4fv("transfer_desat", transfer);

    style->transfer(NPRStyle::COLOR_FADE).toArray(transfer);
    shader.setUniform4fv("transfer_fade", transfer);
    
    shader.setUniform4f("background_color", 
                style->backgroundColor()[0],
                style->backgroundColor()[1],
                style->backgroundColor()[2],
                1.0);

    style->transfer(NPRStyle::FOCUS_TRANSFER).toArray(transfer);
    shader.setUniform4fv("transfer_focus", transfer);

    shader.setUniform1i("focus_mode", NPRSettings::instance().get(NPR_FOCUS_MODE));
    bool lighting_enabled = NPRSettings::instance().get(NPR_ENABLE_LIGHTING);
    if (lighting_enabled)
        shader.setUniform1i("light_mode", scene.light(0)->mode());
    else
        shader.setUniform1i("light_mode", -1);

    float radius = scene.sceneRadius();

    shader.setUniform3fv("w_poa", scene.focalPoint());
    shader.setUniform1f("model_size", radius);

    shader.setUniform3fv("c_poa", camera_poa);
    float cameraFocalLength = fabsf(scene.focalPoint()[2]); // Assumed to be camera coordinates.
    shader.setUniform1f("focal_length", cameraFocalLength);

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    shader.setUniform1f("aspect_ratio", (float)viewport[2] / (float)viewport[3]);
    // transform screen to clip coordinates
    float cx = scene.focalPoint()[0] / ((float)viewport[2]*0.5) - 1;
    float cy = scene.focalPoint()[1] / ((float)viewport[3]*0.5) - 1;
    shader.setUniform2f("s_poa", cx, cy );
}

void NPRRendererStandard::drawDepthBuffer( const NPRScene& scene )
{
    if (NPRSettings::instance().get(NPR_CHECK_LINE_VISIBILITY))
    {
        __TIME_CODE_BLOCK("Draw depth buffer");

        float depth_scale = NPRSettings::instance().get(NPR_SEGMENT_ATLAS_DEPTH_SCALE);
        NPRGLDraw::drawDepthBufferFBO(scene, _depth_buffer, depth_scale );
    }
    else
    {
        _depth_buffer.bind();
        NPRGLDraw::clearGLScreen(vec(1,1,1), 1);
        _depth_buffer.unbind();
    }
}

void NPRRendererStandard::drawPriorityBuffer( const NPRScene& scene )
{
    __TIME_CODE_BLOCK("Draw priority buffer");

    _priority_buffer.initFullScreen(GL_TEXTURE_RECTANGLE_ARB, GL_RGBA, 
                                    GQ_ATTACH_NONE, 1);
    _priority_buffer.setTextureFilter(GL_NEAREST, GL_NEAREST);
    _priority_buffer.bind();
    
    NPRGLDraw::clearGLScreen(vec(1, 1, 1), 1);

    _path_renderer.drawPriorityBuffer(scene, _segment_atlas);

    _priority_buffer.unbind();
}

void NPRRendererStandard::visualizeDepthBuffer()
{
    NPRGLDraw::visualizeFBO(_depth_buffer, 0);
}

void NPRRendererStandard::visualizePriorityBuffer()
{
    NPRGLDraw::visualizeFBO(_priority_buffer, 0);
}
