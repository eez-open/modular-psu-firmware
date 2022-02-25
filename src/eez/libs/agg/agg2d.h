//----------------------------------------------------------------------------
// Agg2D - Version 1.0
// Based on Anti-Grain Geometry
// Copyright (C) 2005 Maxim Shemanarev (http://www.antigrain.com)
//
// Permission to copy, use, modify, sell and distribute this software
// is granted provided this copyright notice appears in all copies.
// This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.
//
//----------------------------------------------------------------------------
// Contact: mcseem@antigrain.com
//          mcseemagg@yahoo.com
//          http://www.antigrain.com
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//
//	25 Jan 2007 - Ported to AGG 2.4 Jerry Evans (jerry@novadsp.com)
//
//----------------------------------------------------------------------------

#ifndef AGG2D_INCLUDED
#define AGG2D_INCLUDED

// With this define uncommented you can use floating-point pixel format
//#define AGG2D_USE_FLOAT_FORMAT

#include "agg_basics.h"
#include "agg_trans_affine.h"
#include "agg_trans_viewport.h"
#include "agg_path_storage.h"
#include "agg_conv_stroke.h"
#include "agg_conv_transform.h"
#include "agg_conv_curve.h"
#include "agg_rendering_buffer.h"
#include "agg_renderer_base.h"
#include "agg_renderer_scanline.h"
#include "agg_span_gradient.h"
#include "agg_span_image_filter_rgba.h"
#include "agg_span_allocator.h"
#include "agg_span_converter.h"
#include "agg_span_interpolator_linear.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_gamma_functions.h"
#include "agg_scanline_u.h"
#include "agg_bezier_arc.h"
#include "agg_rounded_rect.h"

#include "agg_pixfmt_rgba.h"
#include "agg_pixfmt_rgb.h"
#include "agg_pixfmt_rgb_packed.h"
#include "agg_image_accessors.h"

class Agg2D
{
#ifdef AGG2D_USE_FLOAT_FORMAT
    typedef agg::rgba32 ColorType;
#else
    typedef agg::rgba8  ColorType;
#endif
    typedef agg::order_rgba ComponentOrder; // Platform dependent!
    typedef agg::blender_rgba<ColorType, ComponentOrder>             Blender;
    typedef agg::comp_op_adaptor_rgba<ColorType, ComponentOrder>     BlenderComp;
    typedef agg::blender_rgba_pre<ColorType, ComponentOrder>         BlenderPre;
    typedef agg::comp_op_adaptor_rgba_pre<ColorType, ComponentOrder> BlenderCompPre;

#if defined(EEZ_PLATFORM_STM32)
	typedef agg::pixfmt_rgb565         PixFormat;
#else
	typedef agg::pixfmt_alpha_blend_rgba<Blender, agg::rendering_buffer>         PixFormat;
#endif

    typedef agg::pixfmt_custom_blend_rgba<BlenderComp, agg::rendering_buffer>    PixFormatComp;
    typedef agg::pixfmt_alpha_blend_rgba<BlenderPre, agg::rendering_buffer>      PixFormatPre;
    typedef agg::pixfmt_custom_blend_rgba<BlenderCompPre, agg::rendering_buffer> PixFormatCompPre;

    typedef agg::renderer_base<PixFormat>        RendererBase;
    typedef agg::renderer_base<PixFormatComp>    RendererBaseComp;
    typedef agg::renderer_base<PixFormatPre>     RendererBasePre;
    typedef agg::renderer_base<PixFormatCompPre> RendererBaseCompPre;

    typedef agg::renderer_scanline_aa_solid<RendererBase>     RendererSolid;
    typedef agg::renderer_scanline_aa_solid<RendererBaseComp> RendererSolidComp;

    typedef agg::span_allocator<ColorType> SpanAllocator;
    typedef agg::pod_auto_array<ColorType, 256> GradientArray;

    typedef agg::span_gradient<ColorType, agg::span_interpolator_linear<>, agg::gradient_x,      GradientArray> LinearGradientSpan;
    typedef agg::span_gradient<ColorType, agg::span_interpolator_linear<>, agg::gradient_circle, GradientArray> RadialGradientSpan;

    typedef agg::conv_curve<agg::path_storage>    ConvCurve;
    typedef agg::conv_stroke<ConvCurve>           ConvStroke;
    typedef agg::conv_transform<ConvCurve>        PathTransform;
    typedef agg::conv_transform<ConvStroke>       StrokeTransform;

    enum Gradient
    {
        Solid,
        Linear,
        Radial
    };

public:
    friend class Agg2DRenderer;

    // Use srgba8 as the "user" color type, even though the underlying color type 
    // might be something else, such as rgba32. This allows code based on 
    // 8-bit sRGB values to carry on working as before.
    //typedef agg::srgba8       Color;
    typedef agg::rgba8       Color;

    typedef agg::rect_i       Rect;
    typedef agg::rect_d       RectD;
    typedef agg::trans_affine Affine;

    enum LineJoin
    {
        JoinMiter = agg::miter_join,
        JoinRound = agg::round_join,
        JoinBevel = agg::bevel_join
    };

    enum LineCap
    {
        CapButt   = agg::butt_cap,
        CapSquare = agg::square_cap,
        CapRound  = agg::round_cap
    };

    
    enum DrawPathFlag
    {
        FillOnly,
        StrokeOnly,
        FillAndStroke,
        FillWithLineColor
    };

    enum ViewportOption
    {
        Anisotropic,
        XMinYMin,
        XMidYMin,
        XMaxYMin,
        XMinYMid,
        XMidYMid,
        XMaxYMid,
        XMinYMax,
        XMidYMax,
        XMaxYMax
    };

    struct Transformations
    {
        double affineMatrix[6];
    };


    enum BlendMode
    {
        BlendAlpha      = agg::end_of_comp_op_e,
        BlendClear      = agg::comp_op_clear,
        BlendSrc        = agg::comp_op_src,
        BlendDst        = agg::comp_op_dst,
        BlendSrcOver    = agg::comp_op_src_over,
        BlendDstOver    = agg::comp_op_dst_over,
        BlendSrcIn      = agg::comp_op_src_in,
        BlendDstIn      = agg::comp_op_dst_in,
        BlendSrcOut     = agg::comp_op_src_out,
        BlendDstOut     = agg::comp_op_dst_out,
        BlendSrcAtop    = agg::comp_op_src_atop,
        BlendDstAtop    = agg::comp_op_dst_atop,
        BlendXor        = agg::comp_op_xor,
        BlendAdd        = agg::comp_op_plus,
        BlendMultiply   = agg::comp_op_multiply,
        BlendScreen     = agg::comp_op_screen,
        BlendOverlay    = agg::comp_op_overlay,
        BlendDarken     = agg::comp_op_darken,
        BlendLighten    = agg::comp_op_lighten,
        BlendColorDodge = agg::comp_op_color_dodge,
        BlendColorBurn  = agg::comp_op_color_burn,
        BlendHardLight  = agg::comp_op_hard_light,
        BlendSoftLight  = agg::comp_op_soft_light,
        BlendDifference = agg::comp_op_difference,
        BlendExclusion  = agg::comp_op_exclusion,
    };

    enum Direction
    {
        CW, CCW
    };

    ~Agg2D();
    Agg2D();

    // Setup
    //-----------------------
    void  attach(unsigned char* buf, unsigned width, unsigned height, int stride);

    void  clipBox(double x1, double y1, double x2, double y2);
    RectD clipBox() const;

    void  clearAll(Color c);
    void  clearAll(unsigned r, unsigned g, unsigned b, unsigned a = 255);

    void  clearClipBox(Color c);
    void  clearClipBox(unsigned r, unsigned g, unsigned b, unsigned a = 255);

    // Conversions
    //-----------------------
    void   worldToScreen(double& x, double& y) const;
    void   screenToWorld(double& x, double& y) const;
    double worldToScreen(double scalar) const;
    double screenToWorld(double scalar) const;
    void   alignPoint(double& x, double& y) const;
    bool   inBox(double worldX, double worldY) const;

    // General Attributes
    //-----------------------
    void blendMode(BlendMode m);
    BlendMode blendMode() const;

    void masterAlpha(double a);
    double masterAlpha() const;

    void antiAliasGamma(double g);
    double antiAliasGamma() const;

    void fillColor(Color c);
    void fillColor(unsigned r, unsigned g, unsigned b, unsigned a = 255);
    void noFill();

    void lineColor(Color c);
    void lineColor(unsigned r, unsigned g, unsigned b, unsigned a = 255);
    void noLine();

    Color fillColor() const;
    Color lineColor() const;

    void fillLinearGradient(double x1, double y1, double x2, double y2, Color c1, Color c2, double profile=1.0);
    void lineLinearGradient(double x1, double y1, double x2, double y2, Color c1, Color c2, double profile=1.0);

    void fillRadialGradient(double x, double y, double r, Color c1, Color c2, double profile=1.0);
    void lineRadialGradient(double x, double y, double r, Color c1, Color c2, double profile=1.0);

    void fillRadialGradient(double x, double y, double r, Color c1, Color c2, Color c3);
    void lineRadialGradient(double x, double y, double r, Color c1, Color c2, Color c3);

    void fillRadialGradient(double x, double y, double r);
    void lineRadialGradient(double x, double y, double r);

    void lineWidth(double w);
    double lineWidth(double w) const;

    void lineCap(LineCap cap);
    LineCap lineCap() const;

    void lineJoin(LineJoin join);
    LineJoin lineJoin() const;

    void fillEvenOdd(bool evenOddFlag);
    bool fillEvenOdd() const;

    // Transformations
    //-----------------------
    Transformations transformations() const;
    void transformations(const Transformations& tr);
    void resetTransformations();
    void affine(const Affine& tr);
    void affine(const Transformations& tr);
    void rotate(double angle);
    void scale(double sx, double sy);
    void skew(double sx, double sy);
    void translate(double x, double y);
    void parallelogram(double x1, double y1, double x2, double y2, const double* para);
    void viewport(double worldX1,  double worldY1,  double worldX2,  double worldY2,
                  double screenX1, double screenY1, double screenX2, double screenY2,
                  ViewportOption opt=XMidYMid);

    // Basic Shapes
    //-----------------------
    void line(double x1, double y1, double x2, double y2);
    void triangle(double x1, double y1, double x2, double y2, double x3, double y3);
    void rectangle(double x1, double y1, double x2, double y2);
    void roundedRect(double x1, double y1, double x2, double y2, double r);
    void roundedRect(double x1, double y1, double x2, double y2, double rx, double ry);
    void roundedRect(double x1, double y1, double x2, double y2,
                     double rxBottom, double ryBottom,
                     double rxTop,    double ryTop);
    void roundedRect(double x1, double y1, double x2, double y2,
                     double rtlx, double rtly, double rtrx, double rtry,
                     double rbrx, double rbry, double rblx, double rbly);
    void ellipse(double cx, double cy, double rx, double ry);
    void arc(double cx, double cy, double rx, double ry, double start, double sweep);
    void star(double cx, double cy, double r1, double r2, double startAngle, int numRays);
    void curve(double x1, double y1, double x2, double y2, double x3, double y3);
    void curve(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4);
    void polygon(double* xy, int numPoints);
    void polyline(double* xy, int numPoints);


    // Path commands
    //-----------------------
    void resetPath();

    void moveTo(double x, double y);
    void moveRel(double dx, double dy);

    void lineTo(double x, double y);
    void lineRel(double dx, double dy);

    void horLineTo(double x);
    void horLineRel(double dx);

    void verLineTo(double y);
    void verLineRel(double dy);

    void arcTo(double rx, double ry,
               double angle,
               bool largeArcFlag,
               bool sweepFlag,
               double x, double y);

    void arcRel(double rx, double ry,
                double angle,
                bool largeArcFlag,
                bool sweepFlag,
                double dx, double dy);

    void quadricCurveTo(double xCtrl, double yCtrl,
                         double xTo,   double yTo);
    void quadricCurveRel(double dxCtrl, double dyCtrl,
                         double dxTo,   double dyTo);
    void quadricCurveTo(double xTo, double yTo);
    void quadricCurveRel(double dxTo, double dyTo);

    void cubicCurveTo(double xCtrl1, double yCtrl1,
                      double xCtrl2, double yCtrl2,
                      double xTo,    double yTo);

    void cubicCurveRel(double dxCtrl1, double dyCtrl1,
                       double dxCtrl2, double dyCtrl2,
                       double dxTo,    double dyTo);

    void cubicCurveTo(double xCtrl2, double yCtrl2,
                      double xTo,    double yTo);

    void cubicCurveRel(double xCtrl2, double yCtrl2,
                       double xTo,    double yTo);

    void addEllipse(double cx, double cy, double rx, double ry, Direction dir);
    void closePolygon();

    void drawPath(DrawPathFlag flag = FillAndStroke);
    void drawPathNoTransform(DrawPathFlag flag = FillAndStroke);


    // Auxiliary
    //-----------------------
    static double pi() { return agg::pi; }
    static double deg2Rad(double v) { return v * agg::pi / 180.0; }
    static double rad2Deg(double v) { return v * 180.0 / agg::pi; }

private:
    void render(bool fillColor);

    void addLine(double x1, double y1, double x2, double y2);
    void updateRasterizerGamma();

    agg::rendering_buffer           m_rbuf;
    public:
    PixFormat                       m_pixFormat;
    private:
    PixFormatComp                   m_pixFormatComp;
    PixFormatPre                    m_pixFormatPre;
    PixFormatCompPre                m_pixFormatCompPre;
    RendererBase                    m_renBase;
    RendererBaseComp                m_renBaseComp;
    RendererBasePre                 m_renBasePre;
    RendererBaseCompPre             m_renBaseCompPre;
    RendererSolid                   m_renSolid;
    RendererSolidComp               m_renSolidComp;

    SpanAllocator                   m_allocator;
    RectD                           m_clipBox;

    BlendMode                       m_blendMode;

    agg::scanline_u8                m_scanline;
    agg::rasterizer_scanline_aa<>   m_rasterizer;

    double                          m_masterAlpha;
    double                          m_antiAliasGamma;

    Color                           m_fillColor;
    Color                           m_lineColor;
    GradientArray                   m_fillGradient;
    GradientArray                   m_lineGradient;

    LineCap                         m_lineCap;
    LineJoin                        m_lineJoin;

    Gradient                        m_fillGradientFlag;
    Gradient                        m_lineGradientFlag;
    agg::trans_affine               m_fillGradientMatrix;
    agg::trans_affine               m_lineGradientMatrix;
    double                          m_fillGradientD1;
    double                          m_lineGradientD1;
    double                          m_fillGradientD2;
    double                          m_lineGradientD2;

    agg::span_interpolator_linear<> m_fillGradientInterpolator;
    agg::span_interpolator_linear<> m_lineGradientInterpolator;

    agg::gradient_x                 m_linearGradientFunction;
    agg::gradient_circle            m_radialGradientFunction;

    double                          m_lineWidth;
    bool                            m_evenOddFlag;

    agg::path_storage               m_path;
    agg::trans_affine               m_transform;

    ConvCurve                       m_convCurve;
    ConvStroke                      m_convStroke;

    PathTransform                   m_pathTransform;
    StrokeTransform                 m_strokeTransform;
};


inline bool operator == (const Agg2D::Color& c1, const Agg2D::Color& c2)
{
   return c1.r == c2.r && c1.g == c2.g && c1.b == c2.b && c1.a == c2.a;
}

inline bool operator != (const Agg2D::Color& c1, const Agg2D::Color& c2)
{
   return !(c1 == c2);
}


#endif


