//--------------------------------------------------------------
// File name:   makeicon.c
//--------------------------------------------------------------
#include "launchelf.h"
#include "libmc.h"
#include "math.h"
extern u8 font_uLE[];

static u16 *tex_buffer;

#define ICON_WIDTH  128
#define ICON_HEIGHT 128
#define ICON_MARGIN 0
#define FONT_WIDTH  8
#define FONT_HEIGHT 16

// f16 = s16/4096
#define f16 s16
#define f32 float
//--------------------------------------------------------------
struct icon_header
{
    u32 icon_id;       // PS2 icon id = 0x010000
    u32 anim_shapes;   // number of animation shapes
    u32 texture_type;  // 0x07=uncompressed
    u32 UNKNOWN;       // always 0x3F800000
    u32 num_vertices;  // always multiple of 3
};
//--------------------------------------------------------------
struct icon_vertex
{
    f16 xyz[3];
    u16 w;  // no idea what this is //RA: but it is required, though it may be zeroed
};
//--------------------------------------------------------------
struct icon_texturedata
{
    f16 uv[2];
    u8 rgba[4];
};
//--------------------------------------------------------------
struct icon_animheader
{
    u32 id;  // always 0x01
    u32 frame_length;
    f32 anim_speed;
    u32 play_offset;
    u32 num_frames;
};
//--------------------------------------------------------------
struct icon_framedata
{
    u32 shapeid;
    u32 num_keys;
    u32 time;
    u32 value;
};
//--------------------------------------------------------------
// A few cheap defines for assigning vector and texture coordinates to vertices
// Set vector coordinate value of a vertex
#define set_vv(coord, x, y, z) \
    coord.xyz[0] = 4096 * x;   \
    coord.xyz[1] = 4096 * y;   \
    coord.xyz[2] = 4096 * z;   \
    coord.w = 0;
//--------------------------------------------------------------
// Set texture coordinate value of a vertex
#define set_tv(uv, u, v) \
    uv[0] = u * 4096;    \
    uv[1] = v * 4096;
//--------------------------------------------------------------
// draw a char using the system font (16x16)
void tex_drawChar(unsigned int c, int x, int y, u16 colour)
{
    int i, j, k, pixBase, pixMask;
    u8 *cm;

    u16 temp_image[16][32];
    // blank out the memory(was causing issues before)
    for (i = 0; i < 16; i++)
        for (j = 0; j < 32; j++)
            temp_image[i][j] = 0x0000;

    if (c >= 0x11A)
        c = '_';
    cm = &font_uLE[c * 16];  // cm points to the character definition in the font

    pixMask = 0x80;
    for (i = 0; i < 8; i++) {  // for i == each pixel column
        pixBase = -1;
        for (j = 0; j < 16; j++) {                     // for j == each pixel row
            if ((pixBase < 0) && (cm[j] & pixMask)) {  // if start of sequence
                pixBase = j;
            } else if ((pixBase > -1) && !(cm[j] & pixMask)) {  // if end of sequence
                for (k = pixBase; k < j; k++)
                    temp_image[i * 2][k * 2] = 0xFFFF;  // every other pixel is blank
                pixBase = -1;
            }
        }                  // ends for j == each pixel row
        if (pixBase > -1)  // if end of sequence including final row
        {
            for (k = pixBase; k < j; k++)
                temp_image[i * 2][k * 2] = 0xFFFF;
        }
        pixMask >>= 1;
    }  // ends for i == each pixel column

    // OR's with the previous bit in the row to fill in the blank space
    for (i = 1; i < 16; i += 2)
        for (j = 0; j < 32; j++)
            temp_image[i][j] = temp_image[i - 1][j] | temp_image[i][j];

    // OR's with the previous bit in the column to fill in the blank space
    for (j = 1; j < 32; j += 2)
        for (i = 0; i < 16; i++)
            temp_image[i][j] = temp_image[i][j - 1] | temp_image[i][j];

    // store the temp image buffer into the real image buffer
    for (i = 0; i < 16; i++)
        for (j = 0; j < 32; j++)
            tex_buffer[x + i + (y + j) * 128] = temp_image[i][j];
}
//--------------------------------------------------------------
// draw a string of characters to an icon texture, without shift-JIS support
// NOTE: I added in the ability for \n to be part of the string, don't know if its
// necessary though, although there are some cases where it doesnt work

int tex_printXY(const unsigned char *s, int x, int y, u16 colour)
{
    unsigned int c1, c2;
    int i;
    int text_spacing = 16;  // we magnified font
    int x_orig = x;
    int x_max = x;
    i = 0;
    while ((c1 = s[i++]) != 0) {
        if (c1 == '\t') {     //'Horizontal Tab' code ?
            x += FONT_WIDTH;  // use HT to step half a char space, for centering odd chars
            continue;         // loop back to try next character
        }
        if (c1 == '\v')          //'Vertical Tab' code ?
            goto force_halfrow;  // use VT to step half a row down, for centering odd rows
        if (c1 == '\r') {        //'Carriage Return' code ?
        force_newrow:            // use CR to step a full row down, and restart a row
            y += FONT_HEIGHT;
        force_halfrow:  // This label is used to allow centering of odd rows
            y += FONT_HEIGHT;
            if (x > x_max)
                x_max = x;
            x = x_orig;
            continue;  // loop back to try next character
        }
        if (y > ICON_HEIGHT - 2 * FONT_HEIGHT)                // if insufficient room for current row
            break;                                            // then cut the string rendering here
        if (x > ICON_WIDTH - ICON_MARGIN - 2 * FONT_WIDTH) {  // if out of room on current row
            i--;                                              // back index to retry on next loop
            goto force_newrow;                                // and cut this row here.
        }
        // Here we know there is room for at least one char on current row
        if (c1 != 0xFF) {  // Normal character
        norm_char:
            tex_drawChar(c1, x, y, colour);
            x += text_spacing;
            continue;
        }  // End if for normal character
        // Here we got a sequence starting with 0xFF ('\xff')
        if ((c2 = s[i++]) == 0) {  // if that was the final character
            i--;                   // back index to retry on next loop
            goto norm_char;        // and go display '\xff' as any other character
        }
        // Here we deal with any sequence prefixed by '\xff'
        if ((c2 < '0') || (c2 > '='))    // if the sequence is illegal
            continue;                    // then just ignore it
        c1 = (c2 - '0') * 2 + 0x100;     // generate adjusted char code > 0xFF
        tex_drawChar(c1, x, y, colour);  // render this base character to texture
        x += text_spacing;
        if ((c2 > '4') && (c2 < ':'))  // if this is a normal-width character
            continue;                  // continue with the next loop
        // compound sequence "\xff""0"=Circle "\xff""1"=Cross "\xff""2"=Square "\xff""3"=Triangle
        //"\xff""4"=FilledBox "\xff"":"=Pad_Rt "\xff"";"=Pad_Dn "\xff""<"=Pad_Lt "\xff""="=Pad_Up
        if (x > ICON_WIDTH - ICON_MARGIN - 2 * FONT_WIDTH)  // if out of room for compound character ?
            goto force_newrow;                              // then cut this row here.
        tex_drawChar(c1 + 1, x, y, colour);                 // render 2nd half of compound character
        x += text_spacing;
    }  // ends while(1)
    if (x > x_max)
        x_max = x;
    return x_max;  // Return max X position reached (not needed for anything though)
}

//--------------------------------------------------------------
// This is a quick and dirty RLE compression algorithm that assumes
// everything is part of a run, and treats everything as a run,
// regardless as to how long that run is.  It could be slightly
// optimized, but the gains would be rather insignifigant.
// NB: With the text magnification used, the assumption of each
// sequence being a run longer than one pixel is always true.
//--------------------------------------------------------------
u32 tex_compresRLE()
{
    u16 *new_tex = (u16 *)malloc(128 * 128 * 2);
    u16 outbufferpos = 0, runcounter = 0, currentposition = 0;
    while (currentposition < 128 * 128) {
        runcounter = 1;
        // 16384 is size of the uncompressed texture/2
        while (currentposition + runcounter < 16384 && tex_buffer[currentposition] == tex_buffer[currentposition + runcounter])
            runcounter++;
        new_tex[outbufferpos++] = runcounter;
        new_tex[outbufferpos++] = tex_buffer[currentposition];
        currentposition += runcounter;
    }

    u32 size = outbufferpos * 2;
    free(tex_buffer);
    tex_buffer = (u16 *)malloc(size);
    memcpy(tex_buffer, new_tex, size);
    free(new_tex);
    return size;
}
//--------------------------------------------------------------
// These defines multiplied like '4096*+xs' yield a standard sized flat icon
#define xs 5 / 2
#define ys 5 / 2
#define zs 0
//--------------------------------------------------------------
/*
 * Create an icon with the text 'icontext', and store in 'filename'
 * returns 0 on success, -1 on failure
 *
 * The position of the text is hard coded in on line 260
 */
//--------------------------------------------------------------
int make_icon(char *icontext, char *filename)
{
    int i;
    struct icon_header icn_head;
    icn_head.icon_id = 0x010000;
    icn_head.anim_shapes = 0x01;
    icn_head.texture_type = 0x0E;  // uncompressed=0x07
    icn_head.UNKNOWN = 0x3F800000;
    icn_head.num_vertices = 12;

    struct icon_vertex icn_vertices[12];
    struct icon_texturedata texdata[12];
    struct icon_vertex normals[4];  // numvertices/3, as 3 vertices share one normal
    // Back face				//Triangle Vertices   //Texture coordinates of vertices
    set_vv(normals[0], 0, 0, 1);
    set_vv(icn_vertices[0], -xs, -ys, zs);
    set_tv(texdata[0].uv, 1, 0);
    set_vv(icn_vertices[1], xs, -ys, zs);
    set_tv(texdata[1].uv, 0, 0);
    set_vv(icn_vertices[2], xs, ys, zs);
    set_tv(texdata[2].uv, 0, 1);
    set_vv(normals[1], 0, 0, 1);
    set_vv(icn_vertices[3], xs, ys, zs);
    set_tv(texdata[3].uv, 0, 1);
    set_vv(icn_vertices[4], -xs, ys, zs);
    set_tv(texdata[4].uv, 1, 1);
    set_vv(icn_vertices[5], -xs, -ys, zs);
    set_tv(texdata[5].uv, 1, 0);
    // Front face
    set_vv(normals[2], 0, 0, -1);
    set_vv(icn_vertices[6], xs, -ys, -zs);
    set_tv(texdata[6].uv, 1, 0);
    set_vv(icn_vertices[7], -xs, -ys, -zs);
    set_tv(texdata[7].uv, 0, 0);
    set_vv(icn_vertices[8], -xs, ys, -zs);
    set_tv(texdata[8].uv, 0, 1);
    set_vv(normals[3], 0, 0, -1);
    set_vv(icn_vertices[9], -xs, ys, -zs);
    set_tv(texdata[9].uv, 0, 1);
    set_vv(icn_vertices[10], xs, ys, -zs);
    set_tv(texdata[10].uv, 1, 1);
    set_vv(icn_vertices[11], xs, -ys, -zs);
    set_tv(texdata[11].uv, 1, 0);

    for (i = 0; i < icn_head.num_vertices; i++) {
        // the y values are generally too small, make them larger
        icn_vertices[i].xyz[1] -= ys * 4096;  // subtract increases?
        texdata[i].rgba[0] = 0x80;
        texdata[i].rgba[1] = 0x80;
        texdata[i].rgba[2] = 0x80;
        texdata[i].rgba[3] = 0x80;
    }

    struct icon_animheader icn_anim_head;
    icn_anim_head.id = 0x01;
    icn_anim_head.frame_length = 1;
    icn_anim_head.anim_speed = 1;
    icn_anim_head.play_offset = 0;
    icn_anim_head.num_frames = 1;

    struct icon_framedata framedata;
    framedata.shapeid = 0;
    framedata.num_keys = 1;
    framedata.time = 1;
    framedata.value = 1;

    // allocates room for the texture and sets the background to black
    tex_buffer = malloc(128 * 128 * 2);       // the entire 128x128 pixel image(16bpp)
    memset(tex_buffer, 0x00, 128 * 128 * 2);  // black background
    tex_printXY(icontext, 0, 0, 0xFFFF);      // (string,xpos,ypos,color)
    u32 tex_size = tex_compresRLE();          // compress the texture, overwrites tex_buffer

    FILE *f = fopen(filename, "wb");  // open/create the file
    if (f == NULL)
        return -1;
    fwrite(&icn_head, sizeof(icn_head), 1, f);
    for (i = 0; i < icn_head.num_vertices; i++) {
        fwrite(&icn_vertices[i], sizeof(icn_vertices[i]), 1, f);
        fwrite(&normals[i / 3], sizeof(normals[i / 3]), 1, f);
        fwrite(&texdata[i], sizeof(texdata[i]), 1, f);
    }
    fwrite(&icn_anim_head, sizeof(icn_anim_head), 1, f);
    fwrite(&framedata, 1, sizeof(framedata), f);
    fwrite(&tex_size, 4, 1, f);
    fwrite(tex_buffer, 1, tex_size, f);
    fclose(f);
    return 0;
}
//--------------------------------------------------------------
/*
 * This makes the icon.sys file that goes along with the icon itself
 * text is the text that the browser shows (max of 32 characters)
 * iconname is the icon file that it refers to(usually icon.icn/or icon.ico)
 * filename is where to store the icon.sys file(eg: mc0:/FOLDER/icon.sys)
 */
//--------------------------------------------------------------
int make_iconsys(char *title, char *iconname, char *filename)
{
    // mcIcon is defined as part of libmc
    mcIcon icon_sys;

    memset(((void *)&icon_sys), 0, sizeof(icon_sys));

    strcpy(icon_sys.head, "PS2D");
    icon_sys.nlOffset = 0;  // 0=automagically wordwrap, otherwise newline position(multiple of 2)
    strcpy_sjis((short *)&icon_sys.title, title);

    icon_sys.trans = 0x40;
    // default values from mcIconSysGen
    iconIVECTOR bgcolor[4] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
    iconFVECTOR ambientlight = {1, 1, 1, 0};
    iconFVECTOR lightdirection[3] = {
        {0.5, 0.5, 0.5, 0.0},
        {0.0, 0.4, -0.1, 0.0},
        {-0.5, -0.5, 0.5, 0.0},
    };
    iconFVECTOR lightcolors[3] = {
        {0.5, 0.5, 0.5, 0.00},
        {0.7, 0.7, 0.7, 0.00},
        {0.5, 0.5, 0.5, 0.00},
    };
    memcpy(icon_sys.bgCol, bgcolor, sizeof(bgcolor));
    memcpy(icon_sys.lightDir, lightdirection, sizeof(lightdirection));
    memcpy(icon_sys.lightCol, lightcolors, sizeof(lightcolors));
    memcpy(icon_sys.lightAmbient, ambientlight, sizeof(ambientlight));
    strcpy(icon_sys.view, iconname);
    strcpy(icon_sys.copy, iconname);
    strcpy(icon_sys.del, iconname);

    FILE *f = fopen(filename, "wb");  // open/create the file
    if (f == NULL)
        return -1;
    fwrite(&icon_sys, 1, sizeof(icon_sys), f);
    fclose(f);

    return 0;
}
//--------------------------------------------------------------
// End of file: makeicon.c
//--------------------------------------------------------------
