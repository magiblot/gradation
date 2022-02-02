/*
    Gradation Curves Filter v1.45 for VirtualDub -- a wide range of color
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

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

int *rgblab; //LUT Lab
int *labrgb; //LUT Lab

int StartProcImpl(Gradation &grd) {
    if (grd.Labprecalc==0 && grd.process==PROCESS_LAB) { // build up the LUT for the Lab process if it is not precalculated already
        PreCalcLut();
        grd.Labprecalc = 1;}
    return 0;
}

int RunProcImpl(Gradation &grd, int32_t width, int32_t height, uint32_t *src, uint32_t *dst, int32_t src_modulo, int32_t dst_modulo) {
    int32_t w, h;

    int r;
    int g;
    int b;
    int bw;
    int cmin;
    int cdelta;
    int cdeltah;
    int ch;
    int chi;
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

    switch(grd.process)
    {
    case PROCESS_RGB:
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                new_pixel = grd.rvalue[0][(old_pixel & 0xFF0000)>>16] + grd.gvalue[0][(old_pixel & 0x00FF00)>>8] + grd.ovalue[0][(old_pixel & 0x0000FF)];
                *dst++ = new_pixel;
            }
            src = (uint32_t *)((char *)src + src_modulo);
            dst = (uint32_t *)((char *)dst + dst_modulo);
        }
    break;
    case PROCESS_FULL:
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                med_pixel = grd.rvalue[1][(old_pixel & 0xFF0000)>>16] + grd.gvalue[1][(old_pixel & 0x00FF00)>>8] + grd.ovalue[3][(old_pixel & 0x0000FF)];
                new_pixel = grd.rvalue[0][(med_pixel & 0xFF0000)>>16] + grd.gvalue[0][(med_pixel & 0x00FF00)>>8] + grd.ovalue[0][(med_pixel & 0x0000FF)];
                *dst++ = new_pixel;
            }
            src = (uint32_t *)((char *)src + src_modulo);
            dst = (uint32_t *)((char *)dst + dst_modulo);
        }
    break;
    case PROCESS_RGBW:
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
                *dst++ = new_pixel;
            }
            src = (uint32_t *)((char *)src + src_modulo);
            dst = (uint32_t *)((char *)dst + dst_modulo);
        }
    break;
    case PROCESS_FULLW:
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                med_pixel = grd.rvalue[1][(old_pixel & 0xFF0000)>>16] + grd.gvalue[1][(old_pixel & 0x00FF00)>>8] + grd.ovalue[3][(old_pixel & 0x0000FF)];
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
                *dst++ = new_pixel;
            }
            src = (uint32_t *)((char *)src + src_modulo);
            dst = (uint32_t *)((char *)dst + dst_modulo);
        }
    break;
    case PROCESS_OFF:
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
    case PROCESS_YUV:
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                r = ((old_pixel & 0xFF0000)>>16);
                g = ((old_pixel & 0x00FF00)>>8);
                b = (old_pixel & 0x0000FF);
                //RGB to YUV (x=Y y=U z=V)
                x = (32768 + 19595 * r + 38470 * g + 7471 * b)>>16; //correct rounding +32768
                y = (8421375 - 11058 * r - 21710 * g + 32768 * b)>>16; //correct rounding +32768
                z = (8421375 + 32768 * r - 27439 * g - 5329 * b)>>16; //correct rounding +32768
                // Applying the curves
                x = (grd.ovalue[1][x])<<16;
                y = (grd.ovalue[2][y])-128;
                z = (grd.ovalue[3][z])-128;
                // YUV to RGB
                rr = (32768 + x + 91881 * z); //correct rounding +32768
                if (rr<0) {r=0;} else if (rr>16711680) {r=16711680;} else {r = (rr & 0xFF0000);}
                gg = (32768 + x - 22553 * y - 46802 * z); //correct rounding +32768
                if (gg<0) {g=0;} else if (gg>16711680) {g=65280;} else {g = (gg & 0xFF0000)>>8;}
                bb = (32768 + x + 116130 * y); //correct rounding +32768
                if (bb<0) {b=0;} else if (bb>16711680) {b=255;} else {b = bb>>16;}
                new_pixel = (r+g+b);
                *dst++ = new_pixel;
            }
            src = (uint32_t *)((char *)src + src_modulo);
            dst = (uint32_t *)((char *)dst + dst_modulo);
        }
    break;
    case PROCESS_CMYK:
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
                x = grd.ovalue[1][x];
                y = grd.ovalue[2][y];
                z = grd.ovalue[3][z];
                v = grd.ovalue[4][v];
                // CMYK to RGB
                r = 255-((((x*(256-v))+128)>>8)+v); //correct rounding rr+128;
                if (r<0) r=0;
                g = 255-((((y*(256-v))+128)>>8)+v); //correct rounding gg+128;
                if (g<0) g=0;
                b = 255-((((z*(256-v))+128)>>8)+v); //correct rounding bb+128;
                if (b<0) b=0;
                new_pixel = ((r<<16)+(g<<8)+b);
                *dst++ = new_pixel;
            }
            src = (uint32_t *)((char *)src + src_modulo);
            dst = (uint32_t *)((char *)dst + dst_modulo);
        }
    break;
    case PROCESS_HSV:
        for (h = 0; h < height; h++)
        {
            for (w = 0; w < width; w++)
            {
                old_pixel = *src++;
                r = ((old_pixel & 0xFF0000)>>16);
                g = ((old_pixel & 0x00FF00)>>8);
                b = (old_pixel & 0x0000FF);
                //RGB to HSV (x=H y=S z=V)
                cmin = min(r,g);
                cmin = min(b,cmin);
                z = max(r,g);
                z = max(b,z);
                cdelta = z - cmin;
                if( cdelta != 0 )
                {   y = (cdelta*255)/z;
                    cdelta = (cdelta*6);
                    cdeltah = cdelta>>1;
                    if(r==z) {x = (((g-b)<<16)+cdeltah)/cdelta;}
                    else if(g==z) {x = 21845+((((b-r)<<16)+cdeltah)/cdelta);}
                    else {x = 43689+((((r-g)<<16)+cdeltah)/cdelta);}
                    if(x<0) {x=(x+65577)>>8;}
                    else {x=(x+128)>>8;}
                }
                else
                {   y = 0;
                    x = 0;
                }
                // Applying the curves
                x = grd.ovalue[1][x];
                y = grd.ovalue[2][y];
                z = grd.ovalue[3][z];
                // HSV to RGB
                if (y==0)
                {
                    r = z;
                    g = z;
                    b = z;
                }
                else
                {   chi = ((x*6)&0xFF00);
                    ch  = (x*6-chi);;
                    switch(chi)
                    {
                    case 0:
                        r = z;
                        g = (z*(65263-(y*(256-ch)))+65531)>>16;
                        b = (z*(255-y)+94)>>8;
                        break;
                    case 256:
                        r = (z*(65263-y*ch)+65528)>>16;
                        g = z;
                        b = (z*(255-y)+89)>>8;
                        break;
                    case 512:
                        r = (z*(255-y)+89)>>8;
                        g = z;
                        b = (z*(65267-(y*(256-ch)))+65529)>>16;
                        break;
                    case 768:
                        r = (z*(255-y)+89)>>8;
                        g = (z*(65267-y*ch)+65529)>>16;
                        b = z;
                        break;
                    case 1024:
                        r = (z*(65263-(y*(256-ch)))+65528)>>16;
                        g = (z*(255-y)+89)>>8;
                        b = z;
                        break;
                    default:
                        r = z;
                        g = (z*(255-y)+89)>>8;
                        b = (z*(65309-y*(ch+1))+27)>>16;
                        break;
                    }
                }
                new_pixel = ((r<<16)+(g<<8)+b);
                *dst++ = new_pixel;
            }
            src = (uint32_t *)((char *)src + src_modulo);
            dst = (uint32_t *)((char *)dst + dst_modulo);
        }
    break;
    case PROCESS_LAB:
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
                x = grd.ovalue[1][rr];
                y = grd.ovalue[2][gg];
                z = grd.ovalue[3][bb];
                //Lab to XYZ
                new_pixel = labrgb[((x<<16)+(y<<8)+z)];
                *dst++ = new_pixel;
            }
            src = (uint32_t *)((char *)src + src_modulo);
            dst = (uint32_t *)((char *)dst + dst_modulo);
        }
    break;
    }
    return 0;
}

int InitProcImpl(Gradation &grd) {
    int i;

    grd.Labprecalc = 0;
    for (i=0; i<5; i++){
        grd.drwmode[i]=DRAWMODE_SPLINE;
        grd.poic[i]=2;
        grd.drwpoint[i][0][0]=0;
        grd.drwpoint[i][0][1]=0;
        grd.drwpoint[i][1][0]=255;
        grd.drwpoint[i][1][1]=255;}
    grd.process = PROCESS_RGB;
    grd.xl = 300;
    grd.yl = 300;
    grd.offset = 0;
    grd.psel=false;
    grd.cp=0;
    _snprintf(grd.gamma, 10, "%.3lf",1.000);
    for (i=0; i<256; i++) {
        grd.ovalue[0][i] = i;
        grd.rvalue[0][i]=(grd.ovalue[0][i]<<16);
        grd.rvalue[2][i]=(grd.ovalue[0][i]-i)<<16;
        grd.gvalue[0][i]=(grd.ovalue[0][i]<<8);
        grd.gvalue[2][i]=(grd.ovalue[0][i]-i)<<8;
        grd.bvalue[i]=(grd.ovalue[0][i]-i);
        grd.ovalue[1][i] = i;
        grd.rvalue[1][i]=(grd.ovalue[1][i]<<16);
        grd.ovalue[2][i] = i;
        grd.gvalue[1][i]=(grd.ovalue[2][i]<<8);
        grd.ovalue[3][i] = i;
        grd.ovalue[4][i] = i;
    }
    return 0;
}

void CalcCurve(Gradation &grd)
{
    int c1;
    int c2;
    int dx;
    int dy;
    int dxg;
    int dyg;
    int i;
    int j;
    int vy;
    double div;
    double inc;
    double ofs;
    double ga;
    double x[16][16];
    double y[16];
    double a[16];
    double b[16];
    double c[16];

    if (grd.drwpoint[grd.channel_mode][0][0]>0) {for (c2=0;c2<grd.drwpoint[grd.channel_mode][0][0];c2++){grd.ovalue[grd.channel_mode][c2]=grd.drwpoint[grd.channel_mode][0][1];}}
    switch (grd.drwmode[grd.channel_mode]){
        case DRAWMODE_LINEAR:
            for (c1=0; c1<(grd.poic[grd.channel_mode]-1); c1++){
                div=(grd.drwpoint[grd.channel_mode][(c1+1)][0]-grd.drwpoint[grd.channel_mode][c1][0]);
                inc=(grd.drwpoint[grd.channel_mode][(c1+1)][1]-grd.drwpoint[grd.channel_mode][c1][1])/div;
                ofs=grd.drwpoint[grd.channel_mode][c1][1]-inc*grd.drwpoint[grd.channel_mode][c1][0];
                for (c2=grd.drwpoint[grd.channel_mode][c1][0];c2<(grd.drwpoint[grd.channel_mode][(c1+1)][0]+1);c2++)
                {grd.ovalue[grd.channel_mode][c2]=int(c2*inc+ofs+0.5);}
            }
        break;
        case DRAWMODE_SPLINE:
            for (i=0;i<16;i++){ //clear tables
                for (j=0;j<16;j++) {x[i][j]=0;}
                y[i]=0;
                a[i]=0;
                b[i]=0;
                c[i]=0;}

            if (grd.poic[grd.channel_mode]>3) { //curve has more than 3 coordinates
                j=grd.poic[grd.channel_mode]-3; //fill the matrix needed to calculate the b coefficients of the cubic functions an*x^3+bn*x^2+cn*x+dn
                x[0][0]=double(2*(grd.drwpoint[grd.channel_mode][2][0]-grd.drwpoint[grd.channel_mode][0][0]));
                x[0][1]=double((grd.drwpoint[grd.channel_mode][2][0]-grd.drwpoint[grd.channel_mode][1][0]));
                y[0]=3*(double(grd.drwpoint[grd.channel_mode][2][1]-grd.drwpoint[grd.channel_mode][1][1])/double(grd.drwpoint[grd.channel_mode][2][0]-grd.drwpoint[grd.channel_mode][1][0])-double(grd.drwpoint[grd.channel_mode][1][1]-grd.drwpoint[grd.channel_mode][0][1])/double(grd.drwpoint[grd.channel_mode][1][0]-grd.drwpoint[grd.channel_mode][0][0]));
                for (i=1;i<j;i++){
                    x[i][i-1]=double((grd.drwpoint[grd.channel_mode][i+1][0]-grd.drwpoint[grd.channel_mode][i][0]));
                    x[i][i]=double(2*(grd.drwpoint[grd.channel_mode][i+2][0]-grd.drwpoint[grd.channel_mode][i][0]));
                    x[i][i+1]=double((grd.drwpoint[grd.channel_mode][i+2][0]-grd.drwpoint[grd.channel_mode][i+1][0]));
                    y[i]=3*(double(grd.drwpoint[grd.channel_mode][i+2][1]-grd.drwpoint[grd.channel_mode][i+1][1])/double(grd.drwpoint[grd.channel_mode][i+2][0]-grd.drwpoint[grd.channel_mode][i+1][0])-double(grd.drwpoint[grd.channel_mode][i+1][1]-grd.drwpoint[grd.channel_mode][i][1])/double(grd.drwpoint[grd.channel_mode][i+1][0]-grd.drwpoint[grd.channel_mode][i][0]));
                }
                x[j][j-1]=double(grd.drwpoint[grd.channel_mode][j+1][0]-grd.drwpoint[grd.channel_mode][j][0]);
                x[j][j]=double(2*(grd.drwpoint[grd.channel_mode][j+2][0]-grd.drwpoint[grd.channel_mode][j][0]));
                y[j]=3*(double(grd.drwpoint[grd.channel_mode][j+2][1]-grd.drwpoint[grd.channel_mode][j+1][1])/double(grd.drwpoint[grd.channel_mode][j+2][0]-grd.drwpoint[grd.channel_mode][j+1][0])-double(grd.drwpoint[grd.channel_mode][j+1][1]-grd.drwpoint[grd.channel_mode][j][1])/double(grd.drwpoint[grd.channel_mode][j+1][0]-grd.drwpoint[grd.channel_mode][j][0]));

                for (i=0;i<grd.poic[grd.channel_mode]-3;i++) { //resolve the matrix to get the b coefficients
                    div=x[i+1][i]/x[i][i];
                    x[i+1][i]=x[i+1][i]-x[i][i]*div;
                    x[i+1][i+1]=x[i+1][i+1]-x[i][i+1]*div;
                    x[i+1][i+2]=x[i+1][i+2]-x[i][i+2]*div;
                    y[i+1]=y[i+1]-y[i]*div;}
                b[grd.poic[grd.channel_mode]-2]=y[grd.poic[grd.channel_mode]-3]/x[grd.poic[grd.channel_mode]-3][grd.poic[grd.channel_mode]-3]; //last b coefficient
                for (i=grd.poic[grd.channel_mode]-3;i>0;i--) {b[i]=(y[i-1]-x[i-1][i]*b[i+1])/x[i-1][i-1];} // backward subsitution to get the rest of the the b coefficients
            }
            else if (grd.poic[grd.channel_mode]==3) { //curve has 3 coordinates
                b[1]=3*(double(grd.drwpoint[grd.channel_mode][2][1]-grd.drwpoint[grd.channel_mode][1][1])/double(grd.drwpoint[grd.channel_mode][2][0]-grd.drwpoint[grd.channel_mode][1][0])-double(grd.drwpoint[grd.channel_mode][1][1]-grd.drwpoint[grd.channel_mode][0][1])/double(grd.drwpoint[grd.channel_mode][1][0]-grd.drwpoint[grd.channel_mode][0][0]))/double(2*(grd.drwpoint[grd.channel_mode][2][0]-grd.drwpoint[grd.channel_mode][0][0]));}

            for (c2=0;c2<(grd.poic[grd.channel_mode]-1);c2++){ //get the a and c coefficients
                a[c2]=(double(b[c2+1]-b[c2])/double(3*(grd.drwpoint[grd.channel_mode][c2+1][0]-grd.drwpoint[grd.channel_mode][c2][0])));
                c[c2]=double(grd.drwpoint[grd.channel_mode][c2+1][1]-grd.drwpoint[grd.channel_mode][c2][1])/double(grd.drwpoint[grd.channel_mode][c2+1][0]-grd.drwpoint[grd.channel_mode][c2][0])-double(b[c2+1]-b[c2])*double(grd.drwpoint[grd.channel_mode][c2+1][0]-grd.drwpoint[grd.channel_mode][c2][0])/3-b[c2]*(grd.drwpoint[grd.channel_mode][c2+1][0]-grd.drwpoint[grd.channel_mode][c2][0]);}
            for (c1=0;c1<(grd.poic[grd.channel_mode]-1);c1++){ //calculate the y values of the spline curve
                for (c2=grd.drwpoint[grd.channel_mode][(c1)][0];c2<(grd.drwpoint[grd.channel_mode][(c1+1)][0]+1);c2++){
                    vy=int(0.5+a[c1]*(c2-grd.drwpoint[grd.channel_mode][c1][0])*(c2-grd.drwpoint[grd.channel_mode][c1][0])*(c2-grd.drwpoint[grd.channel_mode][c1][0])+b[c1]*(c2-grd.drwpoint[grd.channel_mode][c1][0])*(c2-grd.drwpoint[grd.channel_mode][c1][0])+c[c1]*(c2-grd.drwpoint[grd.channel_mode][c1][0])+grd.drwpoint[grd.channel_mode][c1][1]);
                    if (vy>255) {grd.ovalue[grd.channel_mode][c2]=255;}
                    else if (vy<0) {grd.ovalue[grd.channel_mode][c2]=0;}
                    else {grd.ovalue[grd.channel_mode][c2]=vy;}}
            }
        break;
        case DRAWMODE_GAMMA:
            dx=grd.drwpoint[grd.channel_mode][2][0]-grd.drwpoint[grd.channel_mode][0][0];
            dy=grd.drwpoint[grd.channel_mode][2][1]-grd.drwpoint[grd.channel_mode][0][1];
            dxg=grd.drwpoint[grd.channel_mode][1][0]-grd.drwpoint[grd.channel_mode][0][0];
            dyg=grd.drwpoint[grd.channel_mode][1][1]-grd.drwpoint[grd.channel_mode][0][1];
            ga=log(double(dyg)/double(dy))/log(double(dxg)/double(dx));
            _snprintf(grd.gamma, 10, "%.3lf",(1/ga));
            for (c1=0; c1<dx+1; c1++){
                grd.ovalue[grd.channel_mode][c1+grd.drwpoint[grd.channel_mode][0][0]]=int(0.5+dy*(pow((double(c1)/dx),(ga))))+grd.drwpoint[grd.channel_mode][0][1];
            }
        break;
        default:
        break;
    }
    if (grd.drwpoint[grd.channel_mode][((grd.poic[grd.channel_mode])-1)][0]<255) {for (c2=grd.drwpoint[grd.channel_mode][((grd.poic[grd.channel_mode])-1)][0];c2<256;c2++){grd.ovalue[grd.channel_mode][c2]=grd.drwpoint[grd.channel_mode][(grd.poic[grd.channel_mode]-1)][1];}}
    switch (grd.channel_mode) { //for faster RGB modes
        case 0:
            for (i=0;i<256;i++) {
                grd.rvalue[0][i]=(grd.ovalue[0][i]<<16);
                grd.rvalue[2][i]=(grd.ovalue[0][i]-i)<<16;
                grd.gvalue[0][i]=(grd.ovalue[0][i]<<8);
                grd.gvalue[2][i]=(grd.ovalue[0][i]-i)<<8;
                grd.bvalue[i]=grd.ovalue[0][i]-i;}
        break;
        case 1:
            for (i=0;i<256;i++) {grd.rvalue[1][i]=(grd.ovalue[1][i]<<16);}
        break;
        case 2:
            for (i=0;i<256;i++) {grd.gvalue[1][i]=(grd.ovalue[2][i]<<8);}
        break;
    }
}

void PreCalcLut()
{
    int count;
    int x;
    int y;
    int z;
    int rr;
    int gg;
    int bb;
    int r1;
    int g1;
    int b1;
    int r;
    int g;
    int b;

    rgblab = new int[16777216];
    labrgb = new int[16777216];

    count=0;
    for (r=0; r<256; r++) {
        for (g=0; g<256; g++) {
            for (b=0; b<256; b++) {
                if (r > 10) {rr=int(pow(((r<<4)+224.4),(2.4)));}
                else {rr=int((r<<4)*9987.749);}
                if (g > 10) {gg=int(pow(((g<<4)+224.4),(2.4)));}
                else {gg=int((g<<4)*9987.749);}
                if (b > 10) {bb=int(pow(((b<<4)+224.4),(2.4)));}
                else {bb=int((b<<4)*9987.749);}
                x = int((rr+6.38287545)/12.7657509 + (gg+7.36187255)/14.7237451 + (bb+14.58712555)/29.1742511);
                y = int((rr+12.37891725)/24.7578345 + (gg+3.68093628)/7.36187256 + (bb+36.4678139)/72.9356278);
                z = int((rr+136.1678335)/272.335667 + (gg+22.0856177)/44.1712354 + (bb+2.76970661)/5.53941322);
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
                rgblab[count]=((x<<16)+(y<<8)+z);
                count++;
            }
        }
    }
    count = 0;
    for (x=0; x<256; x++) {
        for (y=0; y<256; y++) {
            for (z=0; z<256; z++) {
                gg=x*50+2040;
                rr=int(y*21.392519204-2549.29163142+gg);
                bb=int(gg-z*58.67940678+7976.6510628);
                if (gg>3060) {g1=int(gg*gg/32352.25239*gg);}
                else {g1=int(x*43413.9788);}
                if (rr>3060) {r1=int(rr*rr/34038.16258*rr);}
                else {r1=int(rr*825.27369-1683558);}
                if (bb>3060) {b1=int(bb*bb/29712.85911*bb);}
                else {b1=int(bb*945.40885-1928634);}
                //XYZ to RGB
                rr = int(r1*16.20355 + g1*-7.6863 + b1*-2.492855);
                gg = int(r1*-4.84629 + g1*9.37995 + b1*0.2077785);
                bb = int(r1*0.278176 + g1*-1.01998 + b1*5.28535);
                if (rr>1565400) {r=int((pow((rr),(0.41666666666666666666666666666667))+7.8297554795)/15.659510959-13.996);}
                else {r=int((rr+75881.7458872)/151763.4917744);}
                if (gg>1565400) {g=int((pow((gg),(0.41666666666666666666666666666667))+7.8297554795)/15.659510959-14.019);}
                else {g=int((gg+75881.7458872)/151763.4917744);}
                if (bb>1565400) {b=int((pow((bb),(0.41666666666666666666666666666667))+7.8297554795)/15.659510959-13.990);}
                else {b=int((bb+75881.7458872)/151763.4917744);}
                if (r<0) {r=0;} else if (r>255) {r=255;}
                if (g<0) {g=0;} else if (g>255) {g=255;}
                if (b<0) {b=0;} else if (b>255) {b=255;}
                labrgb[count]=((r<<16)+(g<<8)+b);
                count++;
            }
        }
    }
}

bool ImportCurve(Gradation &grd) // import curves
{
    FILE *pFile;
    int i;
    int j;
    int stor[1280];
    int lSize;
    int cv;
    bool nrf = false;

    for (i=0;i<5;i++){grd.drwmode[i]=DRAWMODE_PEN;}

    if (grd.filter == 2) // *.acv
    {
        pFile = fopen (grd.filename, "rb");
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
                    if (grd.poic[curpos-1]>16) {grd.poic[curpos-1]=16;}
                    curpos++;
                    cordcount=0;
                    cordpos=i+2;}}
                if (i==cordpos) {
                    grd.drwpoint[curpos-1][cordcount][1]=cv;}
                if (i==(cordpos+2)) {
                    grd.drwpoint[curpos-1][cordcount][0]=cv;
                    if (cordcount<15) {cordcount++;}
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
            grd.cp=0;
            Channel cmtmp=grd.channel_mode;
            for (i=0;i<5;i++) { // calculate curve values
                grd.drwmode[i]=DRAWMODE_SPLINE;
                grd.channel_mode=Channel(i);
                CalcCurve(grd);}
            grd.channel_mode=cmtmp;
            nrf=true;
        }
    }
    if (grd.filter == 3) { // *.csv
        pFile = fopen (grd.filename, "r");
        if (pFile==NULL) {return false;}
        else
        {
            fseek (pFile , 0 , SEEK_END);
            lSize = ftell (pFile);
            rewind (pFile);
            for(i=0; (i < 1280) && ( feof(pFile) == 0 ); i++ )
            {
                fscanf (pFile, "%d", &stor[i]);
            }
            fclose (pFile);
            lSize = lSize/4;
        }
    }
    else if (grd.filter == 4 || grd.filter == 5) { // *.crv *.map
        pFile = fopen (grd.filename, "rb");
        if (pFile==NULL) {return false;}
        else
        {
            int beg = (grd.filter == 4) ? 64 : 320;
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
                    if (grd.poic[curpos]>16) {grd.poic[curpos]=16;} // limit to 16 points
                }
                if (i>=curposnext) { // read raw curve data
                    cordpos=0;
                    if (count<256) {grd.ovalue[curpos][count]=cv;}
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
                    if (cordcount<grd.poic[curpos]-1 && cordcount<15) {cordcount++;} // limit to 16 points
                    cordpos=cordpos+2;}
            }
            fclose (pFile);
        }
        if (grd.filter == 5) { //*.map exchange 4<->0
            int temp[1280];
            int drwtmp[16][2];
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
            for (i=0;i<256;i++) {temp[i]=grd.ovalue[4][i];}
            for (j=4;j>0;j--) {
                for (i=0;i<256;i++) {grd.ovalue[j][i]=grd.ovalue[j-1][i];}
            }
            for (i=0;i<256;i++) {grd.ovalue[0][i]=temp[i];}
        }
        Channel cmtmp=grd.channel_mode;
        for (i=0;i<5;i++) { // calculate curve values
            grd.channel_mode=Channel(i);
            if (grd.drwmode[i]!=DRAWMODE_PEN) {CalcCurve(grd);}
        }
        grd.channel_mode=cmtmp;
        grd.cp=0;
        nrf=true;
    }
    else if (grd.filter == 6) // *.amp Smartvurve hsv
    {
        pFile = fopen (grd.filename, "rb");
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
    else
    {
        pFile = fopen (grd.filename, "rb"); // *.amp
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
                grd.ovalue[0][i] = stor[i];
                grd.rvalue[0][i]=(grd.ovalue[0][i]<<16);
                grd.rvalue[2][i]=(grd.ovalue[0][i]-i)<<16;
                grd.gvalue[0][i]=(grd.ovalue[0][i]<<8);
                grd.gvalue[2][i]=(grd.ovalue[0][i]-i)<<8;
                grd.bvalue[i]=grd.ovalue[0][i]-i;
            }
            for(i=256; i < 512; i++) {
                grd.ovalue[1][(i-256)] = stor[i];
                grd.rvalue[1][(i-256)]=(grd.ovalue[1][(i-256)]<<16);
            }
            for(i=512; i < 768; i++) {
                grd.ovalue[2][(i-512)] = stor[i];
                grd.gvalue[1][(i-512)]=(grd.ovalue[2][(i-512)]<<8);
            }
            for(i=768; i < 1024; i++) {grd.ovalue[3][(i-768)] = stor[i];}
            for(i=1024; i < 1280; i++) {grd.ovalue[4][(i-1024)] = stor[i];}
        }
        if (lSize < 769 && lSize > 256){
            for(i=0; i < 256; i++) {
                grd.ovalue[1][i] = stor[i];
                grd.rvalue[1][i]=(grd.ovalue[1][i]<<16);
            }
            for(i=256; i < 512; i++) {
                grd.ovalue[2][(i-256)] = stor[i];
                grd.gvalue[1][(i-256)]=(grd.ovalue[2][(i-256)]<<8);
            }
            for(i=512; i < 768; i++) {grd.ovalue[3][(i-512)] = stor[i];}
        }
        if (lSize < 257 && lSize > 0) {
            for(i=0; i < 256; i++) {
                grd.ovalue[0][i] = stor[i];
                grd.rvalue[0][i]=(grd.ovalue[0][i]<<16);
                grd.rvalue[2][i]=(grd.ovalue[0][i]-i)<<16;
                grd.gvalue[0][i]=(grd.ovalue[0][i]<<8);
                grd.gvalue[2][i]=(grd.ovalue[0][i]-i)<<8;
                grd.bvalue[i]=grd.ovalue[0][i]-i;
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
void ExportCurve(Gradation &grd) // export curves
{
    FILE *pFile;
    int i;
    int j;

    if (grd.filter == 2) {  // *.acv
        pFile = fopen(grd.filename,"wb");
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
    else if (grd.filter == 3) {  // *.csv
        pFile = fopen (grd.filename,"w");
        for (j=0; j<5;j++) {
            for (i=0; i<256; i++) {
                fprintf (pFile, "%d\n",(grd.ovalue[j][i]));
            }
        }
    }
    else {  // *.amp
        pFile = fopen (grd.filename,"wb");
        for (j=0; j<5;j++) {
            for (i=0; i<256; i++) {
                fputc(grd.ovalue[j][i], pFile);
            }
        }
    }
    fclose(pFile);
}
