/*
    Gradation Curves Filter v1.46 for VirtualDub -- a wide range of color
    manipulation through gradation curves.
    Copyright (C) 2008 Alexander Nagiller
    Speed optimizations for HSV and CMYK by Achim Stahlberger.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "gradation.h"

#include <stdio.h>
#include <math.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

///////////////////////////////////////////////////////////////////////////

static void PreCalcRgb2Lab(int *);
static void PreCalcLab2Rgb(int *);

template <class T>
struct HSV { T h, s, v; };
template <class T>
struct YUV { T y, u, v; };

static inline double interpolateCurveValue(const double y[256], double x);

static HSV<double> rgb2hsv(double r, double g, double b);
static RGB<double> hsv2rgb(double h, double s, double v);
static YUV<double> rgb2yuv(double r, double g, double b);
static RGB<double> yuv2rgb(double y, double u, double v);

///////////////////////////////////////////////////////////////////////////

int rgblab[16777216];
int labrgb[16777216];
static bool labprecalc;

void PreCalcLut(Gradation &grd) {
    if (grd.Labprecalc==0 && grd.process==PROCMODE_LAB) { // build up the LUT for the Lab process if it is not precalculated already
        if (!labprecalc) {
            labprecalc = true;
            PreCalcRgb2Lab(rgblab);
            PreCalcLab2Rgb(labrgb);
        }
        grd.Labprecalc = 1;
    }
}

void Run(const Gradation &grd, int32_t width, int32_t height, uint32_t *src, uint32_t *dst, int32_t src_pitch, int32_t dst_pitch) {
    int32_t w, h;

    int r;
    int g;
    int b;
    int bw;
    int div;
    int divh;
    int v;
    int x;
    int y;
    int z;
    int rr;
    int gg;
    int bb;
    int lab;

    uint32_t old_pixel, new_pixel, med_pixel;
    int32_t src_modulo = src_pitch - width*sizeof(*src);
    int32_t dst_modulo = dst_pitch - width*sizeof(*dst);

    switch(grd.process)
    {
    case PROCMODE_RGB:
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                auto in = unpackRGB(old_pixel);
                auto out =
                    grd.precise ? processIntAsDouble<processRGB>(grd, in.r, in.g, in.b)
                                : processRGBInt(grd, in.r, in.g, in.b);
                new_pixel = packRGB(out) | (old_pixel & 0xFF000000U);
                *dst++ = new_pixel;
            }
            src = (uint32_t *)((char *)src + src_modulo);
            dst = (uint32_t *)((char *)dst + dst_modulo);
        }
    break;
    case PROCMODE_FULL:
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                med_pixel = grd.rvalue[1][(old_pixel & 0xFF0000)>>16] + grd.gvalue[1][(old_pixel & 0x00FF00)>>8] + grd.ovalue(3, old_pixel & 0x0000FF);
                new_pixel = grd.rvalue[0][(med_pixel & 0xFF0000)>>16] + grd.gvalue[0][(med_pixel & 0x00FF00)>>8] + grd.ovalue(0, med_pixel & 0x0000FF);
                *dst++ = new_pixel | (old_pixel & 0xFF000000U);
            }
            src = (uint32_t *)((char *)src + src_modulo);
            dst = (uint32_t *)((char *)dst + dst_modulo);
        }
    break;
    case PROCMODE_RGBW:
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                r = (old_pixel & 0xFF0000);
                g = (old_pixel & 0x00FF00);
                b = (old_pixel & 0x0000FF);
                bw = int((77 * (r >> 16) + 150 * (g >> 8) + 29 * b)>>8);
                    r = r+grd.rvalue[2][bw];
                    if (r<65536) r=0; else if (r>16711680) r=16711680;
                    g = g+grd.gvalue[2][bw];
                    if (g<256) g=0; else if (g>65280) g=65280;
                    b = b+grd.bvalue[bw];
                    if (b<0) b=0; else if (b>255) b=255;
                new_pixel = (r+g+b);
                *dst++ = new_pixel | (old_pixel & 0xFF000000U);
            }
            src = (uint32_t *)((char *)src + src_modulo);
            dst = (uint32_t *)((char *)dst + dst_modulo);
        }
    break;
    case PROCMODE_FULLW:
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                med_pixel = grd.rvalue[1][(old_pixel & 0xFF0000)>>16] + grd.gvalue[1][(old_pixel & 0x00FF00)>>8] + grd.ovalue(3, old_pixel & 0x0000FF);
                r = (med_pixel & 0xFF0000);
                g = (med_pixel & 0x00FF00);
                b = (med_pixel & 0x0000FF);
                bw = int((77 * (r >> 16) + 150 * (g >> 8) + 29 * b)>>8);
                    r = r+grd.rvalue[2][bw];
                    if (r<65536) r=0; else if (r>16711680) r=16711680;
                    g = g+grd.gvalue[2][bw];
                    if (g<256) g=0; else if (g>65280) g=65280;
                    b = b+grd.bvalue[bw];
                    if (b<0) b=0; else if (b>255) b=255;
                new_pixel = (r+g+b);
                *dst++ = new_pixel | (old_pixel & 0xFF000000U);
            }
            src = (uint32_t *)((char *)src + src_modulo);
            dst = (uint32_t *)((char *)dst + dst_modulo);
        }
    break;
    case PROCMODE_OFF:
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                new_pixel = old_pixel;
                *dst++ = new_pixel;
            }
            src = (uint32_t *)((char *)src + src_modulo);
            dst = (uint32_t *)((char *)dst + dst_modulo);
        }
    break;
    case PROCMODE_YUV:
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                auto in = unpackRGB(old_pixel);
                auto out =
                    grd.precise ? processIntAsDouble<processYUV>(grd, in.r, in.g, in.b)
                                : processYUVInt(grd, in.r, in.g, in.b);
                new_pixel = packRGB(out) | (old_pixel & 0xFF000000U);
                *dst++ = new_pixel;
            }
            src = (uint32_t *)((char *)src + src_modulo);
            dst = (uint32_t *)((char *)dst + dst_modulo);
        }
    break;
    case PROCMODE_CMYK:
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                r = ((old_pixel & 0xFF0000)>>16);
                g = ((old_pixel & 0x00FF00)>>8);
                b = (old_pixel & 0x0000FF);
                if(r>=g && r>=b) { /* r is Maximum */
                    v = 255-r;
                    div  = r+1;
                    divh = div>>1;
                    x = 0;
                    y = (((r-g)<<8) + divh)/div;  //correct rounding  yy+(div>>1)
                    z = (((r-b)<<8) + divh)/div;} //correct rounding  zz+(div>>1)
                else if(g>=b) {/* g is maximum */
                    v = 255-g;
                    div  = g+1;
                    divh = div>>1;
                    x = (((g-r)<<8) + divh)/div;  //correct rounding  xx+(div>>1)
                    y = 0;
                    z = (((g-b)<<8) + divh)/div;} //correct rounding  zz+(div>>1)
                else {/* b is maximum */
                    v = 255-b;
                    div  = b+1;
                    divh = div>>1;
                    x = (((b-r)<<8) + divh)/div; //correct rounding  xx+(div>>1)
                    y = (((b-g)<<8) + divh)/div; //correct rounding  yy+(div>>1)
                    z = 0;}
                // Applying the curves
                x = grd.ovalue(1, x);
                y = grd.ovalue(2, y);
                z = grd.ovalue(3, z);
                v = grd.ovalue(4, v);
                // CMYK to RGB
                r = 255-((((x*(256-v))+128)>>8)+v); //correct rounding rr+128;
                if (r<0) r=0;
                g = 255-((((y*(256-v))+128)>>8)+v); //correct rounding gg+128;
                if (g<0) g=0;
                b = 255-((((z*(256-v))+128)>>8)+v); //correct rounding bb+128;
                if (b<0) b=0;
                new_pixel = ((r<<16)+(g<<8)+b);
                *dst++ = new_pixel | (old_pixel & 0xFF000000U);
            }
            src = (uint32_t *)((char *)src + src_modulo);
            dst = (uint32_t *)((char *)dst + dst_modulo);
        }
    break;
    case PROCMODE_HSV:
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                auto in = unpackRGB(old_pixel);
                auto out =
                    grd.precise ? processIntAsDouble<processHSV>(grd, in.r, in.g, in.b)
                                : processHSVInt(grd, in.r, in.g, in.b);
                new_pixel = packRGB(out) | (old_pixel & 0xFF000000U);
                *dst++ = new_pixel;
            }
            src = (uint32_t *)((char *)src + src_modulo);
            dst = (uint32_t *)((char *)dst + dst_modulo);
        }
    break;
    case PROCMODE_LAB:
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                lab = rgblab[(old_pixel & 0xFFFFFF)];
                rr = (lab & 0xFF0000)>>16;
                gg = (lab & 0x00FF00)>>8;
                bb = (lab & 0x0000FF);
                // Applying the curves
                x = grd.ovalue(1, rr);
                y = grd.ovalue(2, gg);
                z = grd.ovalue(3, bb);
                //Lab to XYZ
                new_pixel = labrgb[((x<<16)+(y<<8)+z)];
                *dst++ = new_pixel | (old_pixel & 0xFF000000U);
            }
            src = (uint32_t *)((char *)src + src_modulo);
            dst = (uint32_t *)((char *)dst + dst_modulo);
        }
    break;
    }
}

void Init(Gradation &grd, bool precise) {
    int i;

    grd.precise = precise;
    grd.Labprecalc = 0;
    for (i=0; i<5; i++){
        grd.drwmode[i]=DRAWMODE_SPLINE;
        grd.poic[i]=2;
        grd.drwpoint[i][0][0]=0;
        grd.drwpoint[i][0][1]=0;
        grd.drwpoint[i][1][0]=255;
        grd.drwpoint[i][1][1]=255;}
    grd.process = PROCMODE_RGB;
    sprintf(grd.gamma, "%.3lf", 1.000);
    for (i=0; i<256; i++) {
        grd.ovalue(0, i, i);
        grd.ovalue(1, i, i);
        grd.ovalue(2, i, i);
        grd.ovalue(3, i, i);
        grd.ovalue(4, i, i);
        grd.rvalue[0][i] = i << 16;
        grd.rvalue[1][i] = i << 16;
        grd.rvalue[2][i] = 0 << 16;
        grd.gvalue[0][i] = i << 8;
        grd.gvalue[1][i] = i << 8;
        grd.gvalue[2][i] = 0 << 8;
        grd.bvalue[i] = 0;
    }
}

void CalcCurve(Gradation &grd, Channel channel)
{
    int c1;
    int c2;
    int dx;
    int dy;
    int dxg;
    int dyg;
    int i;
    int j;
    double div;
    double inc;
    double ofs;
    double ga;
    double x[maxPoints][maxPoints];
    double y[maxPoints];
    double a[maxPoints];
    double b[maxPoints];
    double c[maxPoints];

    if (grd.drwpoint[channel][0][0]>0) {
        for (c2=0;c2<grd.drwpoint[channel][0][0];c2++) {
            grd.ovalue(channel, c2, grd.drwpoint[channel][0][1]);
        }
    }
    switch (grd.drwmode[channel]){
        case DRAWMODE_LINEAR:
            for (c1=0; c1<(grd.poic[channel]-1); c1++){
                div=(grd.drwpoint[channel][(c1+1)][0]-grd.drwpoint[channel][c1][0]);
                inc=(grd.drwpoint[channel][(c1+1)][1]-grd.drwpoint[channel][c1][1])/div;
                ofs=grd.drwpoint[channel][c1][1]-inc*grd.drwpoint[channel][c1][0];
                for (c2 = grd.drwpoint[channel][c1][0]; c2 < grd.drwpoint[channel][c1+1][0]+1; ++c2) {
                    grd.ovaluef(channel, c2, c2*inc+ofs);
                }
            }
            break;
        case DRAWMODE_SPLINE:
            for (i=0;i<maxPoints;i++){ //clear tables
                for (j=0;j<maxPoints;j++) {x[i][j]=0;}
                y[i]=0;
                a[i]=0;
                b[i]=0;
                c[i]=0;}

            if (grd.poic[channel]>3) { //curve has more than 3 coordinates
                j=grd.poic[channel]-3; //fill the matrix needed to calculate the b coefficients of the cubic functions an*x^3+bn*x^2+cn*x+dn
                x[0][0]=double(2*(grd.drwpoint[channel][2][0]-grd.drwpoint[channel][0][0]));
                x[0][1]=double((grd.drwpoint[channel][2][0]-grd.drwpoint[channel][1][0]));
                y[0]=3*(double(grd.drwpoint[channel][2][1]-grd.drwpoint[channel][1][1])/double(grd.drwpoint[channel][2][0]-grd.drwpoint[channel][1][0])-double(grd.drwpoint[channel][1][1]-grd.drwpoint[channel][0][1])/double(grd.drwpoint[channel][1][0]-grd.drwpoint[channel][0][0]));
                for (i=1;i<j;i++){
                    x[i][i-1]=double((grd.drwpoint[channel][i+1][0]-grd.drwpoint[channel][i][0]));
                    x[i][i]=double(2*(grd.drwpoint[channel][i+2][0]-grd.drwpoint[channel][i][0]));
                    x[i][i+1]=double((grd.drwpoint[channel][i+2][0]-grd.drwpoint[channel][i+1][0]));
                    y[i]=3*(double(grd.drwpoint[channel][i+2][1]-grd.drwpoint[channel][i+1][1])/double(grd.drwpoint[channel][i+2][0]-grd.drwpoint[channel][i+1][0])-double(grd.drwpoint[channel][i+1][1]-grd.drwpoint[channel][i][1])/double(grd.drwpoint[channel][i+1][0]-grd.drwpoint[channel][i][0]));
                }
                x[j][j-1]=double(grd.drwpoint[channel][j+1][0]-grd.drwpoint[channel][j][0]);
                x[j][j]=double(2*(grd.drwpoint[channel][j+2][0]-grd.drwpoint[channel][j][0]));
                y[j]=3*(double(grd.drwpoint[channel][j+2][1]-grd.drwpoint[channel][j+1][1])/double(grd.drwpoint[channel][j+2][0]-grd.drwpoint[channel][j+1][0])-double(grd.drwpoint[channel][j+1][1]-grd.drwpoint[channel][j][1])/double(grd.drwpoint[channel][j+1][0]-grd.drwpoint[channel][j][0]));

                for (i=0;i<grd.poic[channel]-3;i++) { //resolve the matrix to get the b coefficients
                    div=x[i+1][i]/x[i][i];
                    x[i+1][i]=x[i+1][i]-x[i][i]*div;
                    x[i+1][i+1]=x[i+1][i+1]-x[i][i+1]*div;
                    x[i+1][i+2]=x[i+1][i+2]-x[i][i+2]*div;
                    y[i+1]=y[i+1]-y[i]*div;}
                b[grd.poic[channel]-2]=y[grd.poic[channel]-3]/x[grd.poic[channel]-3][grd.poic[channel]-3]; //last b coefficient
                for (i=grd.poic[channel]-3;i>0;i--) {b[i]=(y[i-1]-x[i-1][i]*b[i+1])/x[i-1][i-1];} // backward subsitution to get the rest of the the b coefficients
            }
            else if (grd.poic[channel]==3) { //curve has 3 coordinates
                b[1]=3*(double(grd.drwpoint[channel][2][1]-grd.drwpoint[channel][1][1])/double(grd.drwpoint[channel][2][0]-grd.drwpoint[channel][1][0])-double(grd.drwpoint[channel][1][1]-grd.drwpoint[channel][0][1])/double(grd.drwpoint[channel][1][0]-grd.drwpoint[channel][0][0]))/double(2*(grd.drwpoint[channel][2][0]-grd.drwpoint[channel][0][0]));}

            for (c2=0;c2<(grd.poic[channel]-1);c2++){ //get the a and c coefficients
                a[c2]=(double(b[c2+1]-b[c2])/double(3*(grd.drwpoint[channel][c2+1][0]-grd.drwpoint[channel][c2][0])));
                c[c2]=double(grd.drwpoint[channel][c2+1][1]-grd.drwpoint[channel][c2][1])/double(grd.drwpoint[channel][c2+1][0]-grd.drwpoint[channel][c2][0])-double(b[c2+1]-b[c2])*double(grd.drwpoint[channel][c2+1][0]-grd.drwpoint[channel][c2][0])/3-b[c2]*(grd.drwpoint[channel][c2+1][0]-grd.drwpoint[channel][c2][0]);}
            for (c1=0;c1<(grd.poic[channel]-1);c1++){ //calculate the y values of the spline curve
                for (c2 = grd.drwpoint[channel][c1][0]; c2 < grd.drwpoint[channel][c1+1][0]+1; ++c2) {
                    double vy = a[c1]*(c2-grd.drwpoint[channel][c1][0])*(c2-grd.drwpoint[channel][c1][0])*(c2-grd.drwpoint[channel][c1][0])+b[c1]*(c2-grd.drwpoint[channel][c1][0])*(c2-grd.drwpoint[channel][c1][0])+c[c1]*(c2-grd.drwpoint[channel][c1][0])+grd.drwpoint[channel][c1][1];
                    grd.ovaluef(channel, c2, MIN(MAX(vy, 0.0), 255.0));
                }
            }
            break;
        case DRAWMODE_GAMMA:
            dx=grd.drwpoint[channel][2][0]-grd.drwpoint[channel][0][0];
            dy=grd.drwpoint[channel][2][1]-grd.drwpoint[channel][0][1];
            dxg=grd.drwpoint[channel][1][0]-grd.drwpoint[channel][0][0];
            dyg=grd.drwpoint[channel][1][1]-grd.drwpoint[channel][0][1];
            ga=log(double(dyg)/double(dy))/log(double(dxg)/double(dx));
            sprintf(grd.gamma, "%.3lf", 1/ga);
            for (c1 = 0; c1 < dx+1; ++c1) {
                grd.ovaluef(channel, c1+grd.drwpoint[channel][0][0], dy*(pow((double(c1)/dx),(ga)))+grd.drwpoint[channel][0][1]);
            }
            break;
        default:
            break;
    }
    if (grd.drwpoint[channel][((grd.poic[channel])-1)][0] < 255) {
        for (c2 = grd.drwpoint[channel][((grd.poic[channel])-1)][0]; c2 < 256; c2++) {
            grd.ovalue(channel, c2, grd.drwpoint[channel][grd.poic[channel]-1][1]);
        }
    }
    for (i = 0; i < 256; ++i) {
        InitRGBValues(grd, channel, i);
    }
}

static void PreCalcRgb2Lab(int *rgblab)
{
    int kk[256];
    for (int i=0; i<256; i++) {
        kk[i] = (i > 10) ? int(pow(((i<<4)+224.4),(2.4))) : int((i<<4)*9987.749);
    }
    for (int r=0; r<256; r++) {
        for (int g=0; g<256; g++) {
            for (int b=0; b<256; b++) {
                int rr = kk[r];
                int gg = kk[g];
                int bb = kk[b];
                int x = int((rr+6.38287545)/12.7657509 + (gg+7.36187255)/14.7237451 + (bb+14.58712555)/29.1742511);
                int y = int((rr+12.37891725)/24.7578345 + (gg+3.68093628)/7.36187256 + (bb+36.4678139)/72.9356278);
                int z = int((rr+136.1678335)/272.335667 + (gg+22.0856177)/44.1712354 + (bb+2.76970661)/5.53941322);
                //XYZ to Lab
                if (x>841776){rr=int(pow((x),(0.33333333333333333333333333333333))*21.9122842);}
                else {rr=int((x+610.28989295)/1220.5797859+1379.3103448275862068965517241379);}
                if (y>885644){gg=int(pow((y),(0.33333333333333333333333333333333))*21.5443498);}
                else {gg=int((y+642.0927467)/1284.1854934+1379.3103448275862068965517241379);}
                if (z>964440){bb=int(pow((z),(0.33333333333333333333333333333333))*20.9408726);}
                else {bb=int((z+699.1298454)/1398.2596908+1379.3103448275862068965517241379);}
                x=int(((gg+16.90331)/33.806620)-40.8);
                y=int(((rr-gg+7.23208898)/14.46417796)+119.167434);
                z=int(((gg-bb+19.837527645)/39.67505529)+135.936123);
                *rgblab++=((x<<16)+(y<<8)+z);
            }
        }
    }
}

static void PreCalcLab2Rgb(int *labrgb)
{
    for (int x=0; x<256; x++) {
        int gg = int(x*50+2040);
        int g1 = (gg > 3060) ? int(gg*gg/32352.25239*gg) : int(x*43413.9788);
        for (int y=0; y<256; y++) {
            int rr = int(y*21.392519204-2549.29163142+gg);
            int r1 = (rr > 3060) ? int(rr*rr/34038.16258*rr) : int(rr*825.27369-1683558);
            for (int z=0; z<256; z++) {
                int bb = int(gg-z*58.67940678+7976.6510628);
                int b1 = (bb > 3060) ? int(bb*bb/29712.85911*bb) : int(bb*945.40885-1928634);
                //XYZ to RGB
                int r = int(r1*16.20355 + g1*-7.6863 + b1*-2.492855);
                int g = int(r1*-4.84629 + g1*9.37995 + b1*0.2077785);
                int b = int(r1*0.278176 + g1*-1.01998 + b1*5.28535);
                if (r>1565400) {r=int((pow((r),(0.41666666666666666666666666666667))+7.8297554795)/15.659510959-13.996);}
                else {r=int((r+75881.7458872)/151763.4917744);}
                if (g>1565400) {g=int((pow((g),(0.41666666666666666666666666666667))+7.8297554795)/15.659510959-14.019);}
                else {g=int((g+75881.7458872)/151763.4917744);}
                if (b>1565400) {b=int((pow((b),(0.41666666666666666666666666666667))+7.8297554795)/15.659510959-13.990);}
                else {b=int((b+75881.7458872)/151763.4917744);}
                if (r<0) {r=0;} else if (r>255) {r=255;}
                if (g<0) {g=0;} else if (g>255) {g=255;}
                if (b<0) {b=0;} else if (b>255) {b=255;}
                *labrgb++=((r<<16)+(g<<8)+b);
            }
        }
    }
}

bool ImportCurve(Gradation &grd, const char *filename, CurveFileType type, DrawMode defDrawMode)
{
    FILE *pFile;
    int i;
    int j;
    int stor[1280];
    int lSize;
    int cv;
    bool nrf = false;

    for (i=0;i<5;i++){grd.drwmode[i]=DRAWMODE_PEN;}

    if (type == FILETYPE_ACV)
    {
        pFile = fopen(filename, "rb");
        if (pFile==NULL) {return false;}
        else
        {
            int noocur;
            int curpos;
            int cordpos = 7;
            int curposnext = -1;
            int cordcount = 0;
            fseek (pFile , 0 , SEEK_END);
            lSize = ftell (pFile);
            rewind (pFile);
            for(i=0; (i < lSize) && ( feof(pFile) == 0 ); i++ ) //read the file and store the coordinates
            {
                cv = fgetc(pFile);
                if (i==3) { noocur = cv;
                    if (noocur>5) {noocur=5;}
                    curpos = 0;}
                if (i==5) {grd.poic[curpos]=cv;
                    if (noocur >= (curpos+1))
                    {curposnext = i+grd.poic[curpos]*4+2;
                    curpos++;}}
                if (i==curposnext) {
                    grd.poic[curpos] = cv;
                    if (noocur >= (curpos+1))
                    {curposnext = i+grd.poic[curpos]*4+2;
                    if (grd.poic[curpos-1]>maxPoints) {grd.poic[curpos-1]=maxPoints;}
                    curpos++;
                    cordcount=0;
                    cordpos=i+2;}}
                if (i==cordpos) {
                    grd.drwpoint[curpos-1][cordcount][1]=cv;}
                if (i==(cordpos+2)) {
                    grd.drwpoint[curpos-1][cordcount][0]=cv;
                    if (cordcount<(maxPoints-1)) {cordcount++;}
                    cordpos=cordpos+4;}
            }
            fclose (pFile);
            if (noocur<5){ //fill empty curves if acv does contain less than 5 curves
                for (i=noocur;i<5;i++)
                    {grd.poic[i]=2;
                    grd.drwpoint[i][0][0]=0;
                    grd.drwpoint[i][0][1]=0;
                    grd.drwpoint[i][1][0]=255;
                    grd.drwpoint[i][1][1]=255;}
                noocur=5;}
            for (i=0;i<5;i++) { // calculate curve values
                grd.drwmode[i]=defDrawMode;
                CalcCurve(grd, Channel(i));
            }
            nrf=true;
        }
    }
    if (type == FILETYPE_CSV) {
        pFile = fopen(filename, "r");
        if (pFile==NULL) {return false;}
        else
        {
            fseek (pFile , 0 , SEEK_END);
            lSize = ftell (pFile);
            rewind (pFile);
            for(i=0; (i < 1280) && ( feof(pFile) == 0 ); i++ )
            {
                if (fscanf(pFile, "%d", &stor[i]) != 1) {
                    stor[i] = '\0';
                }
            }
            fclose (pFile);
            lSize = lSize/4;
        }
    }
    else if (type == FILETYPE_CRV || type == FILETYPE_MAP) {
        pFile = fopen(filename, "rb");
        if (pFile==NULL) {return false;}
        else
        {
            int beg = (type == FILETYPE_CRV) ? 64 : 320;
            int count;
            int curpos = -1;
            int cordpos = beg+6;
            int curposnext = 65530;
            int cordcount;
            int gma = 1;
            fseek (pFile , 0 , SEEK_END);
            lSize = ftell (pFile);
            rewind (pFile);
            for(i=0; (i < lSize) && ( feof(pFile) == 0 ); i++ )
            {
                cv = fgetc(pFile);
                if (i == beg) {
                    curpos++;
                    grd.drwmode[curpos]=DrawMode(cv);
                    curposnext = 65530;
                    if (grd.drwmode[curpos] == DRAWMODE_PEN || grd.drwmode[curpos] == DRAWMODE_SPLINE) {
                        grd.drwmode[curpos] = DrawMode(abs(grd.drwmode[curpos]-2));
                    }
                }
                if (i == beg+1 && grd.drwmode[curpos] == DRAWMODE_GAMMA) {gma=cv;}
                if (i == beg+2 && grd.drwmode[curpos] == DRAWMODE_GAMMA) {gma=gma+(cv<<8);}
                if (i == beg+5) {
                    grd.poic[curpos]=cv;
                    cordpos=i+1;
                    curposnext = i+grd.poic[curpos]*2+1;
                    if (curpos<4) {beg=i+grd.poic[curpos]*2+257;}
                    cordcount=0;
                    count=0;
                    if (grd.poic[curpos]>maxPoints) {grd.poic[curpos]=maxPoints;}
                }
                if (i>=curposnext) { // read raw curve data
                    cordpos=0;
                    if (count<256) {grd.ovalue(curpos, count, cv);}
                    count++;}
                if (i == cordpos) {
                    if (grd.drwmode[curpos] == DRAWMODE_GAMMA && cordcount==1) {
                        if (gma>250) {grd.drwpoint[curpos][cordcount][0]=64;}
                        else if (gma<50) {grd.drwpoint[curpos][cordcount][0]=192;}
                        else {grd.drwpoint[curpos][cordcount][0]=128;}
                        grd.drwpoint[curpos][cordcount][1]=int(pow(float(grd.drwpoint[curpos][cordcount][0])/256,100/float(gma))*256+0.5);
                        cordcount++;
                        grd.poic[curpos]++;}
                    grd.drwpoint[curpos][cordcount][0]=cv;}
                if (i == cordpos+1) {
                    grd.drwpoint[curpos][cordcount][1]=cv;
                    if (cordcount<grd.poic[curpos]-1 && cordcount<maxPoints-1) {cordcount++;}
                    cordpos=cordpos+2;}
            }
            fclose (pFile);
        }
        if (type == FILETYPE_MAP) { //*.map exchange 4<->0
            int temp[1280];
            int drwtmp[maxPoints][2];
            DrawMode drwmodtmp=grd.drwmode[4];
            int pictmp=grd.poic[4];
            for (i=0;i<pictmp;i++){
                drwtmp[i][0]=grd.drwpoint[4][i][0];
                drwtmp[i][1]=grd.drwpoint[4][i][1];}
            for (j=4;j>0;j--) {
                for (i=0;i<grd.poic[j-1];i++) {
                    grd.drwpoint[j][i][0]=grd.drwpoint[j-1][i][0];
                    grd.drwpoint[j][i][1]=grd.drwpoint[j-1][i][1];}
                grd.poic[j]=grd.poic[j-1];
                grd.drwmode[j]=grd.drwmode[j-1];}
            for (i=0;i<pictmp;i++){
                grd.drwpoint[0][i][0]=drwtmp[i][0];
                grd.drwpoint[0][i][1]=drwtmp[i][1];}
            grd.poic[0]=pictmp;
            grd.drwmode[0]=drwmodtmp;
            for (i=0;i<256;i++) {temp[i]=grd.ovalue(4, i);}
            for (j=4;j>0;j--) {
                for (i=0;i<256;i++) {grd.ovalue(j, i, grd.ovalue(j-1, i));}
            }
            for (i=0;i<256;i++) {grd.ovalue(0, i, temp[i]);}
        }
        for (i=0;i<5;i++) { // calculate curve values
            if (grd.drwmode[i] != DRAWMODE_PEN) {
                CalcCurve(grd, Channel(i));
            }
        }
        nrf=true;
    }
    else if (type == FILETYPE_SMARTCURVE_HSV)
    {
        pFile = fopen(filename, "rb");
        if (pFile==NULL) {return false;}
        else
        {
            fseek (pFile , 0 , SEEK_END);
            lSize = ftell (pFile);
            rewind (pFile);
            for(i=0; (i < 768) && ( feof(pFile) == 0 ); i++ )
            {
                if (i<256)
                {stor[i+512] = fgetc(pFile);}
                if (i>255 && i <512)
                {stor[i] = fgetc(pFile);}
                if (i>511)
                {stor[i-512] = fgetc(pFile);}
            }
            fclose (pFile);
            lSize = 768;
        }
    }
    else // FILETYPE_AMP
    {
        pFile = fopen(filename, "rb");
        if (pFile==NULL) {return false;}
        else
        {
            fseek (pFile , 0 , SEEK_END);
            lSize = ftell (pFile);
            rewind (pFile);
            for(i=0; (i < 1280) && ( feof(pFile) == 0 ); i++ )
            {
                stor[i] = fgetc(pFile);
            }
            fclose (pFile);
        }
    }
    if (nrf==false) { //fill curves for non coordinates file types
        if (lSize > 768){
            for(i=0; i < 256; i++) {
                grd.ovalue(0, i, stor[i]);
                grd.rvalue[0][i] = stor[i] << 16;
                grd.rvalue[2][i] = (stor[i] - i) << 16;
                grd.gvalue[0][i] = stor[i] << 8;
                grd.gvalue[2][i] = (stor[i] - i) << 8;
                grd.bvalue[i] = stor[i] - i;
            }
            for(i=256; i < 512; i++) {
                grd.ovalue(1, i-256, stor[i]);
                grd.rvalue[1][(i-256)]=(grd.ovalue(1, i-256)<<16);
            }
            for(i=512; i < 768; i++) {
                grd.ovalue(2, i-512, stor[i]);
                grd.gvalue[1][(i-512)]=(grd.ovalue(2, i-512)<<8);
            }
            for(i=768; i < 1024; i++) {grd.ovalue(3, i-768, stor[i]);}
            for(i=1024; i < 1280; i++) {grd.ovalue(4, i-1024, stor[i]);}
        }
        if (lSize < 769 && lSize > 256){
            for(i=0; i < 256; i++) {
                grd.ovalue(1, i, stor[i]);
                grd.rvalue[1][i]=(grd.ovalue(1, i)<<16);
            }
            for(i=256; i < 512; i++) {
                grd.ovalue(2, i-256, stor[i]);
                grd.gvalue[1][(i-256)]=(grd.ovalue(2, i-256)<<8);
            }
            for(i=512; i < 768; i++) {grd.ovalue(3, i-512, stor[i]);}
        }
        if (lSize < 257 && lSize > 0) {
            for(i=0; i < 256; i++) {
                grd.ovalue(0, i, stor[i]);
                grd.rvalue[0][i] = stor[i] << 16;
                grd.rvalue[2][i] = (stor[i] - i) << 16;
                grd.gvalue[0][i] = stor[i] << 8;
                grd.gvalue[2][i] = (stor[i] - i) << 8;
                grd.bvalue[i] = stor[i] - i;
            }
        }
        for (i=0;i<5;i++) {
            grd.drwmode[i]=DRAWMODE_PEN;
            grd.poic[i]=2;
            grd.drwpoint[i][0][0]=0;
            grd.drwpoint[i][0][1]=0;
            grd.drwpoint[i][1][0]=255;
            grd.drwpoint[i][1][1]=255;
        }
    }
    return true;
}

void ExportCurve(const Gradation &grd, const char *filename, CurveFileType type)
{
    FILE *pFile;
    int i;
    int j;

    if (type == FILETYPE_ACV) {
        pFile = fopen(filename,"wb");
        fputc(0, pFile);
        fputc(4, pFile);
        fputc(0, pFile);
        fputc(5, pFile);
        for (j=0; j<5;j++) {
            fputc(0, pFile);
            fputc(grd.poic[j], pFile);
            for (i=0; i<grd.poic[j]; i++) {
                fputc(0, pFile);
                fputc(grd.drwpoint[j][i][1], pFile);
                fputc(0, pFile);
                fputc(grd.drwpoint[j][i][0], pFile);
            }
        }
    }
    else if (type == FILETYPE_CSV) {
        pFile = fopen(filename,"w");
        for (j=0; j<5;j++) {
            for (i=0; i<256; i++) {
                fprintf(pFile, "%d\n", grd.ovalue(j, i));
            }
        }
    }
    else { // FILETYPE_AMP
        pFile = fopen(filename,"wb");
        for (j=0; j<5;j++) {
            for (i=0; i<256; i++) {
                fputc(grd.ovalue(j, i), pFile);
            }
        }
    }
    fclose(pFile);
}

void ImportPoints(Gradation &grd, Channel channel, const uint8_t points[][2], size_t count, DrawMode drawMode)
{
    if (count != 0)
    {
        for (size_t i = 0; i < MIN(count, maxPoints); ++i)
        {
            grd.drwpoint[channel][i][0] = points[i][0];
            grd.drwpoint[channel][i][1] = points[i][1];
        }
        grd.drwmode[channel] = drawMode;
        grd.poic[channel] = count;
        CalcCurve(grd, channel);
    }
}

static inline double interpolateCurveValue(const double y[256], double x)
{
    // Interpolate from two points.
    uint8_t x1 = uint8_t(x);
    uint8_t x2 = x1 + 1; // Native wrapping: 255 + 1 -> 0.
    double ff = x - x1;
    return y[x1] + ff*(y[x2] - y[x1]);
}

RGB<uint8_t> processRGBInt(const Gradation &grd, uint8_t r, uint8_t g, uint8_t b)
{
    return unpackRGB(
        grd.rvalue[0][r] +
        grd.gvalue[0][g] +
        grd.ovalue(0, b)
    );
}

RGB<double> processRGB(const Gradation &grd, double r, double g, double b)
{
    return {
        interpolateCurveValue(grd.ovaluef(0), r),
        interpolateCurveValue(grd.ovaluef(0), g),
        interpolateCurveValue(grd.ovaluef(0), b),
    };
}

RGB<uint8_t> processHSVInt(const Gradation &grd, uint8_t r, uint8_t g, uint8_t b)
{
    // RGB to HSV
    uint8_t h, s, v;
    uint8_t cmin = MIN(MIN(r, g), b);
    v = MAX(MAX(r, g), b);
    int32_t cdelta = v - cmin;
    if (cdelta != 0)
    {
        s = uint8_t((cdelta*255)/v);
        cdelta = (cdelta*6);
        int32_t cdeltah = cdelta >> 1;
        int32_t x;
        if (r == v)
            x = ((int32_t(g - b) << 16) + cdeltah)/cdelta;
        else if (g == v)
            x = 21845 + ((int32_t(b - r) << 16) + cdeltah)/cdelta;
        else
            x = 43689 + ((int32_t(r - g) << 16) + cdeltah)/cdelta;
        if (x < 0)
            h = uint8_t((x + 65577) >> 8);
        else
            h = uint8_t((x + 128) >> 8);
    }
    else
        h = s = 0;
    // Apply the curves
    h = grd.ovalue(1, h);
    s = grd.ovalue(2, s);
    v = grd.ovalue(3, v);
    // HSV to RGB
    if (s == 0)
        return {v, v, v};
    int32_t chi = ((h*6) & 0xFF00);
    int32_t ch = (h*6 - chi);
    switch (chi)
    {
        case 0:
            r = v;
            g = uint8_t((v*(65263 - (s*(256 - ch))) + 65531) >> 16);
            b = uint8_t((v*(255 - s) + 94) >> 8);
            break;
        case 256:
            r = uint8_t((v*(65263 - s*ch) + 65528) >> 16);
            g = v;
            b = uint8_t((v*(255 - s) + 89) >> 8);
            break;
        case 512:
            r = uint8_t((v*(255 - s) + 89) >> 8);
            g = v;
            b = uint8_t((v*(65267 - (s*(256 - ch))) + 65529) >> 16);
            break;
        case 768:
            r = uint8_t((v*(255 - s) + 89) >> 8);
            g = uint8_t((v*(65267 - s*ch) + 65529) >> 16);
            b = v;
            break;
        case 1024:
            r = uint8_t((v*(65263 - (s*(256 - ch))) + 65528) >> 16);
            g = uint8_t((v*(255 - s) + 89) >> 8);
            b = v;
            break;
        default:
            r = v;
            g = uint8_t((v*(255 - s) + 89) >> 8);
            b = uint8_t((v*(65309 - s*(ch + 1)) + 27) >> 16);
            break;
    }
    return {r, g, b};
}

RGB<double> processHSV(const Gradation &grd, double r, double g, double b)
{
    auto hsv = rgb2hsv(r, g, b);
    auto rgb = hsv2rgb(
        interpolateCurveValue(grd.ovaluef(1), hsv.h),
        interpolateCurveValue(grd.ovaluef(2), hsv.s),
        interpolateCurveValue(grd.ovaluef(3), hsv.v)
    );
    return rgb;
}

// https://stackoverflow.com/a/6930407
static HSV<double> rgb2hsv(double r, double g, double b)
{
    double min = MIN(MIN(r, g), b);
    double max = MAX(MAX(r, g), b);
    double delta = max - min;

    HSV<double> out;
    out.v = max;

    if (max == 0.0)
        out.s = 0;
    else
        out.s = delta*255.0/max;

    if (delta == 0.0)
        out.h = 0;
    else if (r == max)
        out.h = (g - b)/delta;
    else if (g == max)
        out.h = 2.0 + (b - r)/delta;
    else
        out.h = 4.0 + (r - g)/delta;

    out.h *= 42.5;

    if (out.h < 0.0)
        out.h += 255.0;

    return out;
}

static RGB<double> hsv2rgb(double h, double s, double v)
{
    if (s == 0.0)
        return {v, v, v};

    double hh = h < 255.0 ? h/42.5 : 0.0;
    int i = (int) hh;
    double ff = hh - i;
    s = s/255.0;
    double p = v * (1.0 - s);
    double q = v * (1.0 - (s * ff));
    double t = v * (1.0 - (s * (1.0 - ff)));

    switch (i)
    {
        case 0:     return {v, t, p};
        case 1:     return {q, v, p};
        case 2:     return {p, v, t};
        case 3:     return {p, q, v};
        case 4:     return {t, p, v};
        default:    return {v, p, q};
    }
}

RGB<uint8_t> processYUVInt(const Gradation &grd, uint8_t r, uint8_t g, uint8_t b)
{
    //RGB to YUV (x=Y y=U z=V)
    int x, y, z;
    x = (32768 + 19595 * r + 38470 * g + 7471 * b)>>16; //correct rounding +32768
    y = (8421375 - 11058 * r - 21710 * g + 32768 * b)>>16; //correct rounding +32768
    z = (8421375 + 32768 * r - 27439 * g - 5329 * b)>>16; //correct rounding +32768
    // Applying the curves
    x = (grd.ovalue(1, x))<<16;
    y = (grd.ovalue(2, y))-128;
    z = (grd.ovalue(3, z))-128;
    // YUV to RGB
    int rr = (32768 + x + 91881 * z)>>16; //correct rounding +32768
    int gg = (32768 + x - 22553 * y - 46802 * z)>>16; //correct rounding +32768
    int bb = (32768 + x + 116130 * y)>>16; //correct rounding +32768
    return {
        (uint8_t) MIN(MAX(rr, 0), 255),
        (uint8_t) MIN(MAX(gg, 0), 255),
        (uint8_t) MIN(MAX(bb, 0), 255),
    };
}

RGB<double> processYUV(const Gradation &grd, double r, double g, double b)
{
    auto yuv = rgb2yuv(r, g, b);
    auto rgb = yuv2rgb(
        interpolateCurveValue(grd.ovaluef(1), yuv.y),
        interpolateCurveValue(grd.ovaluef(2), yuv.u),
        interpolateCurveValue(grd.ovaluef(3), yuv.v)
    );
    return rgb;
}

namespace BT601
{
constexpr auto mR = 0.299;
constexpr auto mG = 0.587;
constexpr auto mB = 0.114;
constexpr auto dCB = 1.772;
constexpr auto dCR = 1.402;
}

static YUV<double> rgb2yuv(double r, double g, double b)
{
    using namespace BT601;
    double y = mR*r + mG*g + mB*b;
    double u = 128 + (b - y)/dCB;
    double v = 128 + (r - y)/dCR;
    return {
        MIN(MAX(y, 0.0), 255.0),
        MIN(MAX(u, 0.0), 255.0),
        MIN(MAX(v, 0.0), 255.0),
    };
}

static RGB<double> yuv2rgb(double y, double u, double v)
{
    using namespace BT601;
    u -= 128; v -= 128;
    double r = v*dCR + y;
    double g = y + -(mB*dCB/mG)*u -(mR*dCR/mG)*v;
    double b = u*dCB + y;
    return {
        MIN(MAX(r, 0.0), 255.0),
        MIN(MAX(g, 0.0), 255.0),
        MIN(MAX(b, 0.0), 255.0),
    };
}
