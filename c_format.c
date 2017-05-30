#include <stdio.h>
#include <stdlib.h>

#include "main.h"
#include "misc.h"
#include "format.h"
#include "logging.h"

// C format output functions

static void c_open_output(output_t *out, const char *input, bool header) {
    if (input) {
        FILE *file;
        if (!(file = fopen(input, "w"))) {
            errorf("opening %s for output.", input);
        }

        if (header) {
            out->h = file;
        } else {
            out->c = file;
        }
    }
}

static void c_close_output(output_t *out, bool header) {
    if (header) {
        if (out->h) { fclose(out->h); }
    } else {
        if (out->c) { fclose(out->c); }
    }
}

static void c_print_source_header(output_t *out, const char *header_file_name) {
    fprintf(out->c, "// Converted using ConvPNG\n");
    fprintf(out->c, "#include <stdint.h>\n");
    if (header_file_name) {
        fprintf(out->c, "#include \"%s\"\n\n", header_file_name);
    }
}

static void c_print_header_header(output_t *out, const char *group_name) {
    fprintf(out->h, "// Converted using ConvPNG\n");
    fprintf(out->h, "// This file contains all the graphics sources for easier inclusion in a project\n");
    fprintf(out->h, "#ifndef __%s__\n#define __%s__\n", group_name, group_name);
    fprintf(out->h, "#include <stdint.h>\n\n");
}

static void c_print_palette(output_t *out, const char *group_name, liq_palette *pal, const unsigned int pal_len) {
    fprintf(out->c, "uint16_t %s_pal[%u] = {\n", group_name, pal_len);
     
    for (unsigned int j = 0; j < pal_len; j++) {
        liq_color *e = &pal->entries[j];
        fprintf(out->c, " 0x%04X,  // %02u :: rgba(%u,%u,%u,%u)\n", rgb1555(e->r, e->g, e->b), j, e->r, e->g, e->b, e->a);
    }
    fprintf(out->c, "};");
}

static void c_print_transparent_index(output_t *out, const char *group_name, const unsigned int index) {
    fprintf(out->h, "#define %s_transparent_color_index %u\n\n", group_name, index);
}

static void c_print_image_source_header(output_t *out, const char *group_header_file_name) {
    fprintf(out->c, "// Converted using ConvPNG\n");
    fprintf(out->c, "#include <stdint.h>\n");
    fprintf(out->c, "#include \"%s\"\n\n", group_header_file_name);
}

static void c_print_tile(output_t *out, const char *i_name, unsigned int tile_num, unsigned int size, uint8_t width, uint8_t height) {
    fprintf(out->c, "uint8_t %s_tile_%u_data[%u] = {\n %u,\t// tile_width\n %u,\t// tile_height\n ", i_name, tile_num, size, width, height);
}

static void c_print_tile_ptrs(output_t *out, const char *i_name, unsigned int num_tiles, bool compressed) {
    unsigned int i = 0;

    if (compressed) {
        fprintf(out->c, "uint8_t *%s_tiles_compressed[%u] = {\n", i_name, num_tiles);
        for (; i < num_tiles; i++) {
            fprintf(out->c, " %s_tile_%u_compressed,\n", i_name, i);
        }
    } else {
        fprintf(out->c, "uint8_t *%s_tiles_data[%u] = {\n", i_name, num_tiles);
        for (; i < num_tiles; i++) {
            fprintf(out->c, " %s_tile_%u_data,\n", i_name, i);
        }
    }
    fprintf(out->c, "};\n");
}

static void c_print_compressed_tile(output_t *out, const char *i_name, unsigned int tile_num, unsigned int size) {
    fprintf(out->c, "uint8_t %s_tile_%u_compressed[%u] = {\n", i_name, tile_num, size);
}

static void c_print_byte(output_t *out, uint8_t byte, bool need_comma) {
    (void)need_comma;
    fprintf(out->c, "0x%02X,", byte);
}

static void c_print_next_array_line(output_t *out, bool at_end) {
    if (at_end) {
        fprintf(out->c, "\n};\n");
    } else {
        fprintf(out->c, "\n ");
    }
}

static void c_print_image(output_t *out, uint8_t bpp, const char *i_name, unsigned int size, const uint8_t width, const uint8_t height) {
    fprintf(out->c, "// %u bpp image\nuint8_t %s_data[%u] = {\n %u,%u,  // width,height\n ", bpp, i_name, size, width, height);
}

static void c_print_compressed_image(output_t *out, uint8_t bpp, const char *i_name, unsigned int size) {
    fprintf(out->c, "// %u bpp image\nuint8_t %s_compressed[%u] = {\n ", bpp, i_name, size);
}

static void c_print_tiles_header(output_t *out, const char *i_name, unsigned int num_tiles, bool compressed) {
    unsigned int i = 0;
    if (compressed) {
        for (; i < num_tiles; i++) {
            fprintf(out->h, "extern uint8_t %s_tile_%u_compressed[];\n", i_name, i);
        }
    } else {
        for (; i < num_tiles; i++) {
            fprintf(out->h,"extern uint8_t %s_tile_%u_data[];\n", i_name, i);
            fprintf(out->h, "#define %s_tile_%u ((gfx_image_t*)%s_tile_%u_data)\n", i_name, i, i_name, i);
        }
    }
}

static void c_print_tiles_ptrs_header(output_t *out, const char *i_name, unsigned int num_tiles, bool compressed) {
    if (compressed) {
        fprintf(out->h, "extern uint8_t *%s_tiles_compressed[%u];\n", i_name, num_tiles);
    } else {
        fprintf(out->h, "extern uint8_t *%s_tiles_data[%u];\n", i_name, num_tiles);
        fprintf(out->h, "#define %s_tiles ((gfx_image_t**)%s_tiles_data)\n", i_name, i_name);
    }
}

static void c_print_image_header(output_t *out, const char *i_name, unsigned int size, bool compressed) {
    if (compressed) {
        fprintf(out->h, "extern uint8_t %s_compressed[%u];\n", i_name, size);
    } else {
        fprintf(out->h, "extern uint8_t %s_data[%u];\n", i_name, size);
        fprintf(out->h, "#define %s ((gfx_image_t*)%s_data)\n", i_name, i_name);
    }
}

static void c_print_transparent_image_header(output_t *out, const char *i_name, unsigned int size, bool compressed) {
    if (compressed) {
        fprintf(out->h, "extern uint8_t %s_compressed[%u];\n", i_name, size);
    } else {
        fprintf(out->h, "extern uint8_t %s_data[%u];\n", i_name, size);
        fprintf(out->h, "#define %s ((gfx_timage_t*)%s_data)\n", i_name, i_name);
    }
}

static void c_print_palette_header(output_t *out, const char *name, unsigned int len) {
    fprintf(out->h, "#define sizeof_%s_pal %u\n", name, len*2);
    fprintf(out->h, "extern uint16_t %s_pal[%u];\n", name, len);
}

static void c_print_end_header(output_t *out) {
    fprintf(out->h, "\n#endif\n");
}

static void c_print_appvar_array(output_t *out, const char *a_name, unsigned int num_images) {
    fprintf(out->c, "uint8_t *%s[%u] = {\n ", a_name, num_images);
    fprintf(out->h,"#include <stdbool.h>\n\n");
    fprintf(out->h, "#define %s_num %u\n\n", a_name, num_images);
    fprintf(out->h, "extern uint8_t *%s[%u];\n", a_name, num_images);
}

static void c_print_appvar_image(output_t *out, const char *a_name, unsigned int offset, const char *i_name, unsigned int index, bool compressed, bool tp_style) {
    const char *s = "gfx_image_t";
    if (tp_style) {
        s = "gfx_timage_t";
    }
    fprintf(out->c, "(uint8_t*)%u,", offset);
    if (compressed) {
        fprintf(out->h, "#define %s_compressed ((%s*)%s[%u])\n", i_name, s, a_name, index);
    } else {
        fprintf(out->h, "#define %s ((%s*)%s[%u])\n", i_name, s, a_name, index);
    }
}

static void c_print_appvar_palette(output_t *out, unsigned int offset) {
    fprintf(out->c, "(uint8_t*)%u,", offset);
}

static void c_print_appvar_load_function_header(output_t *out) {
    fprintf(out->c, "#include <fileioc.h>\n");
}

static void c_print_appvar_load_function(output_t *out, const char *a_name) {
    fprintf(out->c, "\nbool %s_init(void) {\n", a_name);
    fprintf(out->c, "    unsigned int i;\n");
    fprintf(out->c, "    ti_var_t appvar;\n");
    fprintf(out->c, "    void *data;\n\n");
    fprintf(out->c, "    ti_CloseAll();\n\n");
    fprintf(out->c, "    appvar = ti_Open(\"%s\", \"r\");\n", a_name);
    fprintf(out->c, "    data = ti_GetDataPtr(appvar);\n");
    fprintf(out->c, "    for (i = 0; i < %s_num; i++) {\n", a_name);
    fprintf(out->c, "        %s[i] += (unsigned int)data - (unsigned int)%s[0];\n", a_name, a_name);
    fprintf(out->c, "    }\n\n");
    fprintf(out->c, "    ti_CloseAll();\n");
    fprintf(out->c, "    return (bool)appvar;\n");
    fprintf(out->c, "}\n");
    fprintf(out->h, "\nbool %s_init(void);\n", a_name);
}

static void c_print_appvar_palette_header(output_t *out, const char *p_name, const char *a_name, unsigned int index, unsigned int len) {
    fprintf(out->h, "#define sizeof_%s_pal %u\n", p_name, len*2);
    fprintf(out->h, "#define %s_pal ((uint16_t*)%s[%u])\n", p_name, a_name, index);
}

const format_t c_format = {
    .open_output = c_open_output,
    .close_output = c_close_output,
    .print_source_header = c_print_source_header,
    .print_header_header = c_print_header_header,
    .print_palette = c_print_palette,
    .print_transparent_index = c_print_transparent_index,
    .print_image_source_header = c_print_image_source_header,
    .print_tile = c_print_tile,
    .print_compressed_tile = c_print_compressed_tile,
    .print_tile_ptrs = c_print_tile_ptrs,
    .print_byte = c_print_byte,
    .print_next_array_line = c_print_next_array_line,
    .print_image = c_print_image,
    .print_compressed_image = c_print_compressed_image,
    .print_tiles_header = c_print_tiles_header,
    .print_tiles_ptrs_header = c_print_tiles_ptrs_header,
    .print_image_header = c_print_image_header,
    .print_transparent_image_header = c_print_transparent_image_header,
    .print_palette_header = c_print_palette_header,
    .print_end_header = c_print_end_header,
    .print_appvar_array = c_print_appvar_array,
    .print_appvar_image = c_print_appvar_image,
    .print_appvar_load_function_header = c_print_appvar_load_function_header,
    .print_appvar_load_function = c_print_appvar_load_function,
    .print_appvar_palette_header = c_print_appvar_palette_header,
    .print_appvar_palette = c_print_appvar_palette,
}; 

