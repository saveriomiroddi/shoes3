//
// shoes/ruby.c
// Just little bits of Ruby I've become accustomed to.
//
#include "shoes/app.h"
#include "shoes/canvas.h"
#include "shoes/ruby.h"
#include "shoes/internal.h"
#include "shoes/world.h"
#include "shoes/native/native.h"
#include "shoes/version.h"
#include "shoes/types/types.h"
#include <math.h>

VALUE cShoes, cApp, cDialog, cTypes, cShoesWindow, cMouse, cCanvas, cFlow, cStack, cMask, cWidget, cImage, cPattern, cBorder, cBackground, cTextBlock, cPara, cBanner, cTitle, cSubtitle, cTagline, cCaption, cInscription, cTextClass, cProgress, cColor, cResponse, ssNestSlot;
VALUE eImageError, eInvMode, eNotImpl;
VALUE reHEX_SOURCE, reHEX3_SOURCE, reRGB_SOURCE, reRGBA_SOURCE, reGRAY_SOURCE, reGRAYA_SOURCE, reLF;
VALUE symAltQuest, symAltSlash, symAltDot, symAltEqual, symAltSemiColon;
ID s_perc, s_fraction, s_aref, s_mult, s_donekey;

SYMBOL_DEFS(SYMBOL_ID);

//
// Mauricio's instance_eval hack (he bested my cloaker back in 06 Jun 2006)
//
VALUE instance_eval_proc;

VALUE mfp_instance_eval(VALUE obj, VALUE block) {
    return rb_funcall(instance_eval_proc, s_call, 2, obj, block);
}

//
// From Guy Decoux [ruby-talk:144098]
//
static VALUE ts_each(VALUE *tmp) {
    return rb_funcall2(tmp[0], (ID)tmp[1], (int)tmp[2], (VALUE *)tmp[3]);
}

VALUE ts_funcall2(VALUE obj, ID meth, int argc, VALUE *argv) {
    VALUE tmp[4];
    if (!rb_block_given_p())
        return rb_funcall2(obj, meth, argc, argv);
    tmp[0] = obj;
    tmp[1] = (VALUE)meth;
    tmp[2] = (VALUE)argc;
    tmp[3] = (VALUE)argv;
    return rb_iterate((VALUE(*)(VALUE))ts_each, (VALUE)tmp, CASTHOOK(rb_yield), 0);
}

#define SET_ARG(o) args->a[n] = o, n = n + 1
#define CHECK_ARG(mac, t, d, cond, condarg) \
{ \
  if ((!x || m) && n < argc) \
  { \
    if (mac(argv[n]) == t) \
      SET_ARG(argv[n]); \
    else if (cond) \
      SET_ARG(condarg); \
    else \
    { \
      if (m) m = 0; \
      x = 1; \
    } \
  } \
  else if (m) \
    SET_ARG(d); \
  else \
    x = 1; \
}
#define CHECK_ARG_TYPE(t, d) CHECK_ARG(TYPE, t, d, 0, Qnil)
#define CHECK_ARG_NOT_NIL()  CHECK_ARG(NIL_P, 0, Qnil, 0, Qnil)
#define CHECK_ARG_COERCE(t, meth) \
  CHECK_ARG(TYPE, t, Qnil, rb_respond_to(argv[n], s_##meth), rb_funcall(argv[n], s_##meth, 0))
#define CHECK_ARG_DATA(func) CHECK_ARG(TYPE, T_DATA, Qnil, func(argv[n]), argv[n])

//
// rb_parse_args
// - a rb_scan_args replacement, designed to assist in typecasting, since
//   use of RSTRING_* and RARRAY_* macros are so common in Shoes.
//
// returns 0 if no match.
// returns 1 and up, the arg list matched.
// (args.n is set to the arg count, args.a is the args)
//
static int rb_parse_args_p(unsigned char rais, int argc, const VALUE *argv, const char *fmt, rb_arg_list *args) {
    int i = 1, m = 0, n = 0, nmin = 0, x = 0;
    const char *p = fmt;
    args->n = 0;

    do {
        if (*p == ',') {
            if ((x && !m) || n < argc) {
                i++;
                x = 0;
                if (nmin == 0 || nmin > n) {
                    nmin = n;
                }
                n = 0;
            } else break;
        } else if (*p == '|') {
            if (!x) m = i;
        } else if (*p == 's') {
            CHECK_ARG_COERCE(T_STRING, to_str)
        } else if (*p == 'S') {
            CHECK_ARG_COERCE(T_STRING, to_s)
        } else if (*p == 'i') {
            CHECK_ARG_COERCE(T_FIXNUM, to_int)
        } else if (*p == 'I') {
            CHECK_ARG_COERCE(T_FIXNUM, to_i)
        } else if (*p == 'f') {
            CHECK_ARG_TYPE(T_FLOAT, Qnil)
        } else if (*p == 'F') {
            CHECK_ARG_COERCE(T_FLOAT, to_f)
        } else if (*p == 'a') {
            CHECK_ARG_COERCE(T_ARRAY, to_ary)
        } else if (*p == 'A') {
            CHECK_ARG_COERCE(T_ARRAY, to_a)
        } else if (*p == 'k') {
            CHECK_ARG_TYPE(T_CLASS, Qnil)
        } else if (*p == 'h') {
            CHECK_ARG_TYPE(T_HASH, Qnil)
        }  else if (*p == 'o') {
            CHECK_ARG_NOT_NIL()
        } else if (*p == '&') {
            if (rb_block_given_p())
                SET_ARG(rb_block_proc());
            else
                SET_ARG(Qnil);
        }

        //
        // shoes-specific structures
        //
        else if (*p == 'e') {
            CHECK_ARG_DATA(shoes_is_element)
        } else if (*p == 'E') {
            CHECK_ARG_DATA(shoes_is_any)
        } else break;
    } while (p++);

    if (!x && n >= argc)
        m = i;
    if (m)
        args->n = n;

    // printf("rb_parse_args(%s): %d %d (%d)\n", fmt, m, n, x);

    if (!m && rais) {
        if (argc < nmin)
            rb_raise(rb_eArgError, "wrong number of arguments (%d for %d)", argc, nmin);
        else
            rb_raise(rb_eArgError, "bad arguments");
    }

    return m;
}

int rb_parse_args(int argc, const VALUE *argv, const char *fmt, rb_arg_list *args) {
    return rb_parse_args_p(1, argc, argv, fmt, args);
}

int rb_parse_args_allow(int argc, const VALUE *argv, const char *fmt, rb_arg_list *args) {
    return rb_parse_args_p(0, argc, argv, fmt, args);
}

long rb_ary_index_of(VALUE ary, VALUE val) {
    long i;

    for (i=0; i<RARRAY_LEN(ary); i++) {
        if (rb_equal(RARRAY_PTR(ary)[i], val))
            return i;
    }

    return -1;
}

VALUE rb_ary_insert_at(VALUE ary, long index, int len, VALUE ary2) {
    rb_funcall(ary, s_aref, 3, LONG2NUM(index), INT2NUM(len), ary2);
    return ary;
}

//
// from ruby's eval.c
//
inline VALUE call_cfunc(HOOK func, VALUE recv, int len, int argc, VALUE *argv) {
    if (len >= 0 && argc != len) {
        rb_raise(rb_eArgError, "wrong number of arguments (%d for %d)",
                 argc, len);
    }

    switch (len) {
        case -2:
            return (*func)(recv, rb_ary_new4(argc, argv));
        case -1:
            return (*func)(argc, argv, recv);
        case 0:
            return (*func)(recv);
        case 1:
            return (*func)(recv, argv[0]);
        case 2:
            return (*func)(recv, argv[0], argv[1]);
        case 3:
            return (*func)(recv, argv[0], argv[1], argv[2]);
        case 4:
            return (*func)(recv, argv[0], argv[1], argv[2], argv[3]);
        case 5:
            return (*func)(recv, argv[0], argv[1], argv[2], argv[3], argv[4]);
        case 6:
            return (*func)(recv, argv[0], argv[1], argv[2], argv[3], argv[4],
                           argv[5]);
        case 7:
            return (*func)(recv, argv[0], argv[1], argv[2], argv[3], argv[4],
                           argv[5], argv[6]);
        case 8:
            return (*func)(recv, argv[0], argv[1], argv[2], argv[3], argv[4],
                           argv[5], argv[6], argv[7]);
        case 9:
            return (*func)(recv, argv[0], argv[1], argv[2], argv[3], argv[4],
                           argv[5], argv[6], argv[7], argv[8]);
        case 10:
            return (*func)(recv, argv[0], argv[1], argv[2], argv[3], argv[4],
                           argv[5], argv[6], argv[7], argv[8], argv[9]);
        case 11:
            return (*func)(recv, argv[0], argv[1], argv[2], argv[3], argv[4],
                           argv[5], argv[6], argv[7], argv[8], argv[9], argv[10]);
        case 12:
            return (*func)(recv, argv[0], argv[1], argv[2], argv[3], argv[4],
                           argv[5], argv[6], argv[7], argv[8], argv[9],
                           argv[10], argv[11]);
        case 13:
            return (*func)(recv, argv[0], argv[1], argv[2], argv[3], argv[4],
                           argv[5], argv[6], argv[7], argv[8], argv[9], argv[10],
                           argv[11], argv[12]);
        case 14:
            return (*func)(recv, argv[0], argv[1], argv[2], argv[3], argv[4],
                           argv[5], argv[6], argv[7], argv[8], argv[9], argv[10],
                           argv[11], argv[12], argv[13]);
        case 15:
            return (*func)(recv, argv[0], argv[1], argv[2], argv[3], argv[4],
                           argv[5], argv[6], argv[7], argv[8], argv[9], argv[10],
                           argv[11], argv[12], argv[13], argv[14]);
        default:
            rb_raise(rb_eArgError, "too many arguments (%d)", len);
            break;
    }
    return Qnil;        /* not reached */
}

typedef struct {
    VALUE canvas;
    VALUE block;
    VALUE args;
} safe_block;

static VALUE shoes_safe_block_call(VALUE rb_sb) {
    int i;
    VALUE vargs[10];
    safe_block *sb = (safe_block *)rb_sb;
    for (i = 0; i < RARRAY_LEN(sb->args); i++)
        vargs[i] = rb_ary_entry(sb->args, i);
    return rb_funcall2(sb->block, s_call, (int)RARRAY_LEN(sb->args), vargs);
}

static VALUE shoes_safe_block_exception(VALUE rb_sb, VALUE e) {
    safe_block *sb = (safe_block *)rb_sb;
    shoes_canvas_error(sb->canvas, e);
    return Qnil;
}

VALUE shoes_safe_block(VALUE self, VALUE block, VALUE args) {
    safe_block sb;
    VALUE v;

    sb.canvas = shoes_find_canvas(self);
    sb.block = block;
    sb.args = args;
    rb_gc_register_address(&args);

    v = rb_rescue2(CASTHOOK(shoes_safe_block_call), (VALUE)&sb,
                   CASTHOOK(shoes_safe_block_exception), (VALUE)&sb, rb_cObject, 0);
    rb_gc_unregister_address(&args);
    return v;
}


/* get a dimension, in pixels, given a string, float, int or nil
**      "90%" or 0.9 or actual dimension as integer or
**      amount of pixel to substract from base to compute value
**      or nil
** int dv : default value
** int pv : a base dimension to process
** int nv : switch (boolean) to substract or not computed value from base dimension
*/

int shoes_px(VALUE obj, int dv, int pv, int nv) {
    int px;
    if (TYPE(obj) == T_STRING) {
        char *ptr = RSTRING_PTR(obj);
        int len = (int)RSTRING_LEN(obj);
        obj = rb_funcall(obj, s_to_i, 0);
        if (len > 1 && ptr[len - 1] == '%') {
            obj = rb_funcall(obj, s_mult, 1, rb_float_new(0.01));
        }
    }
    if (rb_obj_is_kind_of(obj, rb_cFloat)) {
        px = (int)((double)pv * NUM2DBL(obj));
    } else {
        if (NIL_P(obj))
            px = dv;
        else
            px = NUM2INT(obj);
        if (px < 0 && nv == 1)
            px += pv;
    }
    return px;
}

/* get a coordinate, in pixels, given bounds (left/right or top/bottom)
** int dv : default value
** int pv : a base dimension to process
** int dr : a delta to substract if working with :right or :bottom
*/

int shoes_px2(VALUE attr, ID k1, ID k2, int dv, int dr, int pv) {
    int px;
    VALUE obj = shoes_hash_get(attr, k2);
    if (!NIL_P(obj)) {
        px = shoes_px(obj, 0, pv, 0);
        px = (pv - dr) - px;
    } else {
        px = shoes_px(shoes_hash_get(attr, k1), dv, pv, 0);
    }
    return px;
}

VALUE shoes_hash_set(VALUE hsh, ID key, VALUE val) {
    if (NIL_P(hsh))
        hsh = rb_hash_new();
    rb_hash_aset(hsh, ID2SYM(key), val);
    return hsh;
}

VALUE shoes_hash_get(VALUE hsh, ID key) {
    VALUE v;

    if (TYPE(hsh) == T_HASH) {
        v = rb_hash_aref(hsh, ID2SYM(key));
        if (!NIL_P(v)) return v;
    }

    return Qnil;
}

int shoes_hash_int(VALUE hsh, ID key, int dn) {
    VALUE v = shoes_hash_get(hsh, key);
    if (!NIL_P(v)) return NUM2INT(v);
    return dn;
}

double shoes_hash_dbl(VALUE hsh, ID key, double dn) {
    VALUE v = shoes_hash_get(hsh, key);
    if (!NIL_P(v)) return NUM2DBL(v);
    return dn;
}

char * shoes_hash_cstr(VALUE hsh, ID key, char *dn) {
    VALUE v = shoes_hash_get(hsh, key);
    if (!NIL_P(v)) return RSTRING_PTR(v);
    return dn;
}

VALUE rb_str_to_pas(VALUE str) {
    VALUE str2;
    char slen[2];
    slen[0] = RSTRING_LEN(str);
    slen[1] = '\0';
    str2 = rb_str_new2(slen);
    rb_str_append(str2, str);
    return str2;
}

void shoes_place_exact(shoes_place *place, VALUE attr, int ox, int oy) {
    int r;
    VALUE x;
    place->dx = ATTR2(int, attr, displace_left, 0);
    place->dy = ATTR2(int, attr, displace_top, 0);
    place->flags = FLAG_ABSX | FLAG_ABSY;
    place->ix = place->x = ATTR2(int, attr, left, 0) + ox;
    place->iy = place->y = ATTR2(int, attr, top, 0) + oy;
    r = ATTR2(int, attr, radius, 0) * 2;
    place->iw = place->w = ATTR2(int, attr, width, r);
    place->ih = place->h = ATTR2(int, attr, height, place->w);
    x = ATTR(attr, right);
    if (!NIL_P(x)) place->iw = place->w = (NUM2INT(x) + ox) - place->x;
    x = ATTR(attr, bottom);
    if (!NIL_P(x)) place->ih = place->h = (NUM2INT(x) + oy) - place->y;

    if (RTEST(ATTR(attr, center))) {
        place->ix = place->x = place->x - (place->w / 2);
        place->iy = place->y = place->y - (place->h / 2);
    }
}

void shoes_place_decide(shoes_place *place, VALUE c, VALUE attr, int dw, int dh, unsigned char rel, int padded) {
    shoes_canvas *canvas = NULL;
    if (!NIL_P(c)) Data_Get_Struct(c, shoes_canvas, canvas);
    VALUE ck = rb_obj_class(c);
    VALUE stuck = ATTR(attr, attach);

    // for image : we want to scale the image, given only one attribute :width or :height
    // get dw and dh, set width or height
    if (REL_FLAGS(rel) & REL_SCALE) {   // 8
        VALUE rw = ATTR(attr, width), rh = ATTR(attr, height);

        if (NIL_P(rw) && !NIL_P(rh)) {          // we have height
            // fetch height in pixels whatever the input (string, float, positive/negative int)
            int spx = shoes_px(rh, dh, CPH(canvas), 1);
            // compute width with image aspect ratio [(dh == dw) means a square ]
            dw = (dh == dw) ? spx : ROUND(((dh * 1.) / dw) * spx);
            dh = spx;                           // now re-init 'dh' for next calculations
            ATTRSET(attr, width, INT2NUM(dw));  // set calculated width
        } else if (NIL_P(rh) && !NIL_P(rw)) {
            int spx = shoes_px(rw, dw, CPW(canvas), 1);
            dh = (dh == dw) ? spx : ROUND(((dh * 1.) / dw) * spx);
            dw = spx;
            ATTRSET(attr, height, INT2NUM(dh));
        }
    }

    ATTR_MARGINS(attr, 0, canvas);
    if (padded || dh == 0) dh += tmargin + bmargin;
    if (padded || dw == 0) dw += lmargin + rmargin;

    int testw = dw;
    if (testw == 0) testw = lmargin + 1 + rmargin;

    if (!NIL_P(stuck)) {
        if (stuck == cShoesWindow)
            rel = REL_FLAGS(rel) | REL_WINDOW;
        else if (stuck == cMouse)
            rel = REL_FLAGS(rel) | REL_CURSOR;
        else
            rel = REL_FLAGS(rel) | REL_STICKY;
    }

    place->flags = rel;
    place->dx = place->dy = 0;
    if (canvas == NULL) {
        place->ix = place->x = 0;
        place->iy = place->y = 0;
        place->iw = place->w = dw;
        place->ih = place->h = dh;
    } else {
        int cx, cy, ox, oy, tw = dw, th = dh;

        switch (REL_COORDS(rel)) {
            case REL_WINDOW:
                cx = 0;
                cy = 0;
                ox = 0;
                oy = canvas->slot->scrolly;
                break;

            case REL_CANVAS:
                cx = canvas->cx - CPX(canvas);
                cy = canvas->cy - CPY(canvas);
                ox = CPX(canvas);
                oy = CPY(canvas);
                break;

            case REL_CURSOR:
                cx = 0;
                cy = 0;
                ox = canvas->app->mousex;
                oy = canvas->app->mousey;
                break;

            case REL_TILE:
                cx = 0;
                cy = 0;
                ox = CPX(canvas);
                oy = CPY(canvas);
                testw = dw = CPW(canvas);
                dh = max(canvas->height, CPH(canvas));
                // Fix #2 ?
                //dh = (max(canvas->height, canvas->fully - CPB(canvas)) - CPY(canvas));
                break;

            default:
                cx = 0;
                cy = 0;
                ox = canvas->cx;
                oy = canvas->cy;
                if ((REL_COORDS(rel) & REL_STICKY) && shoes_is_element(stuck)) {
                    shoes_element *element;
                    Data_Get_Struct(stuck, shoes_element, element);
                    ox = element->place.x;
                    oy = element->place.y;
                }
                break;
        }

        place->w = PX(attr, width, testw, CPW(canvas));
        if (dw == 0 && place->w + (int)canvas->cx > canvas->place.iw) {
            canvas->cx = canvas->endx = CPX(canvas);
            canvas->cy = canvas->endy;
            place->w = canvas->place.iw;
        }
        place->h = PX(attr, height, dh, CPH(canvas));

        if (REL_COORDS(rel) != REL_TILE) {
            tw = place->w;
            th = place->h;
        }
        place->x = PX2(attr, left, right, cx, tw, canvas->place.iw) + ox;
        place->y = PX2(attr, top, bottom, cy, th,
                       ORIGIN(canvas->place) ? canvas->height : canvas->fully) + oy;
        if (!ORIGIN(canvas->place)) {
            place->dx = canvas->place.dx;
            place->dy = canvas->place.dy;
        }
        place->dx += PXN(attr, displace_left, 0, CPW(canvas));
        place->dy += PXN(attr, displace_top, 0, CPH(canvas));

        place->flags |= NIL_P(ATTR(attr, left)) && NIL_P(ATTR(attr, right)) ? 0 : FLAG_ABSX;
        place->flags |= NIL_P(ATTR(attr, top)) && NIL_P(ATTR(attr, bottom)) ? 0 : FLAG_ABSY;
        if (REL_COORDS(rel) != REL_TILE && ABSY(*place) == 0 && (ck == cStack || place->x + place->w > CPX(canvas) + canvas->place.iw)) {
            canvas->cx = place->x = CPX(canvas);
            canvas->cy = place->y = canvas->endy;
        }
    }
    place->ix = place->x + lmargin;
    place->iy = place->y + tmargin;
    place->iw = place->w - (lmargin + rmargin);
    //place->iw = (RTEST(ATTR(attr, width))) ? place->w : place->w - (lmargin + rmargin);
    if (place->iw < 0) place->iw = 0;
    place->ih = place->h - (tmargin + bmargin);
    //place->ih = (RTEST(ATTR(attr, height))) ? place->h : place->h - (tmargin + bmargin);
    if (place->ih < 0) place->ih = 0;

    INFO("PLACE: (%d, %d), (%d: %d, %d: %d) [%d, %d] %x\n", place->x, place->y, place->w, place->iw, place->h, place->ih, ABSX(*place), ABSY(*place), place->flags);

}

//
// shoes_basic routines
//
VALUE shoes_basic_remove(VALUE self) {
    GET_STRUCT(basic, self_t);
    shoes_canvas_remove_item(self_t->parent, self, 0, 0);
    shoes_canvas_repaint_all(self_t->parent);
    return self;
}

unsigned char shoes_is_element_p(VALUE ele, unsigned char any) {
    void *dmark;
    if (TYPE(ele) != T_DATA)
        return 0;
    dmark = RDATA(ele)->dmark;
    return (dmark == shoes_canvas_mark || dmark == shoes_shape_mark ||
            dmark == shoes_image_mark || dmark == shoes_effect_mark ||
            dmark == shoes_pattern_mark || dmark == shoes_textblock_mark ||
            dmark == shoes_control_mark ||
            (any && (dmark == shoes_http_mark || dmark == shoes_timer_mark ||
                     dmark == shoes_color_mark || dmark == shoes_link_mark ||
                     dmark == shoes_text_mark))
           );
}

unsigned char shoes_is_element(VALUE ele) {
    return shoes_is_element_p(ele, 0);
}

unsigned char shoes_is_any(VALUE ele) {
    return shoes_is_element_p(ele, 1);
}

void shoes_extras_remove_all(shoes_canvas *canvas) {
    int i;
    shoes_basic *basic;
    shoes_canvas *parent;
    if (canvas->app == NULL) return;
    for (i = (int)RARRAY_LEN(canvas->app->extras) - 1; i >= 0; i--) {
        VALUE ele = rb_ary_entry(canvas->app->extras, i);
        if (!NIL_P(ele)) {
            Data_Get_Struct(ele, shoes_basic, basic);
            Data_Get_Struct(basic->parent, shoes_canvas, parent);
            if (parent == canvas) {
                rb_funcall(ele, s_remove, 0);
                rb_ary_delete_at(canvas->app->extras, i);
            }
        }
    }
}

void shoes_ele_remove_all(VALUE contents) {
    if (!NIL_P(contents)) {
        long i;
        VALUE ary;
        ary = rb_ary_dup(contents);
        rb_gc_register_address(&ary);
        for (i = 0; i < RARRAY_LEN(ary); i++)
            if (!NIL_P(rb_ary_entry(ary, i)))
                rb_funcall(rb_ary_entry(ary, i), s_remove, 0);
        rb_gc_unregister_address(&ary);
        rb_ary_clear(contents);
    }
}

void shoes_cairo_arc(cairo_t *cr, double x, double y, double w, double h, double a1, double a2) {
    cairo_translate(cr, x, y);
    cairo_scale(cr, w / 2., h / 2.);
    cairo_arc(cr, 0., 0., 1., a1, a2);
}

void shoes_cairo_rect(cairo_t *cr, double x, double y, double w, double h, double r) {
    double rc = r * BEZIER;
    cairo_move_to(cr, x + r, y);
    cairo_rel_line_to(cr, w - 2 * r, 0.0);
    if (r != 0.) cairo_rel_curve_to(cr, rc, 0.0, r, rc, r, r);
    cairo_rel_line_to(cr, 0, h - 2 * r);
    if (r != 0.) cairo_rel_curve_to(cr, 0.0, rc, rc - r, r, -r, r);
    cairo_rel_line_to(cr, -w + 2 * r, 0);
    if (r != 0.) cairo_rel_curve_to(cr, -rc, 0, -r, -rc, -r, -r);
    cairo_rel_line_to(cr, 0, -h + 2 * r);
    if (r != 0.) cairo_rel_curve_to(cr, 0.0, -rc, r - rc, -r, r, -r);
    cairo_close_path(cr);
}

//
// Macros for setting up drawing
//
#define SETUP(self_type, rel, dw, dh) \
  self_type *self_t; \
  shoes_place place; \
  shoes_canvas *canvas; \
  Data_Get_Struct(self, self_type, self_t); \
  Data_Get_Struct(c, shoes_canvas, canvas); \
  if (ATTR(self_t->attr, hidden) == Qtrue) return self; \
  shoes_place_decide(&place, c, self_t->attr, dw, dh, rel, REL_COORDS(rel) == REL_CANVAS)

// SETUP_CONTROL and FINISH macro are moved to ruby.h

//
// Shoes::Image
//
void shoes_image_mark(shoes_image *image) {
    rb_gc_mark_maybe(image->path);
    rb_gc_mark_maybe(image->parent);
    rb_gc_mark_maybe(image->attr);
}

static void shoes_image_free(shoes_image *image) {
    if (image->type == SHOES_CACHE_MEM) {
        if (image->cr != NULL)     cairo_destroy(image->cr);
        if (image->cached != NULL) cairo_surface_destroy(image->cached->surface);
        SHOE_FREE(image->cached);
    }
    shoes_transform_release(image->st);
    RUBY_CRITICAL(SHOE_FREE(image));
}

VALUE shoes_image_new(VALUE klass, VALUE path, VALUE attr, VALUE parent, shoes_transform *st) {
    VALUE obj = Qnil;
    shoes_image *image;
    shoes_basic *basic;
    Data_Get_Struct(parent, shoes_basic, basic);

    obj = shoes_image_alloc(klass);
    Data_Get_Struct(obj, shoes_image, image);

    image->path = Qnil;
    image->st = shoes_transform_touch(st);
    image->attr = attr;
    image->parent = shoes_find_canvas(parent);
    COPY_PENS(image->attr, basic->attr);

    if (rb_obj_is_kind_of(path, cImage)) {
        shoes_image *image2;
        Data_Get_Struct(path, shoes_image, image2);
        image->cached = image2->cached;
        image->type = SHOES_CACHE_ALIAS;
    } else if (!NIL_P(path)) {
        path = shoes_native_to_s(path);
        image->path = path;
        image->cached = shoes_load_image(image->parent, path);
        image->type = SHOES_CACHE_FILE;
    } else {
        shoes_canvas *canvas;
        Data_Get_Struct(image->parent, shoes_canvas, canvas);
        int w = ATTR2(int, attr, width, canvas->width);
        int h = ATTR2(int, attr, height, canvas->height);
        VALUE block = ATTR(attr, draw);
        image->cached = shoes_cached_image_new(w, h, NULL);
        image->cr = cairo_create(image->cached->surface);
        image->type = SHOES_CACHE_MEM;
        if (!NIL_P(block)) DRAW(obj, canvas->app, rb_funcall(block, s_call, 0));
    }

    return obj;
}

VALUE shoes_image_alloc(VALUE klass) {
    VALUE obj;
    shoes_image *image = SHOE_ALLOC(shoes_image);
    SHOE_MEMZERO(image, shoes_image, 1);
    obj = Data_Wrap_Struct(klass, shoes_image_mark, shoes_image_free, image);
    image->path = Qnil;
    image->st = NULL;
    image->attr = Qnil;
    image->parent = Qnil;
    image->type = SHOES_CACHE_MEM;
    return obj;
}

VALUE shoes_image_get_full_width(VALUE self) {
    GET_STRUCT(image, image);
    return INT2NUM(image->cached->width);
}

VALUE shoes_image_get_full_height(VALUE self) {
    GET_STRUCT(image, image);
    return INT2NUM(image->cached->height);
}

void shoes_image_ensure_dup(shoes_image *image) {
    if (image->type == SHOES_CACHE_MEM)
        return;
    shoes_cached_image *cached = shoes_cached_image_new(image->cached->width, image->cached->height, NULL);
    image->cr = cairo_create(cached->surface);
    cairo_set_source_surface(image->cr, image->cached->surface, 0, 0);
    cairo_paint(image->cr);

    image->cached = cached;
    image->type = SHOES_CACHE_MEM;
}

unsigned char *shoes_image_surface_get_pixel(shoes_cached_image *cached, int x, int y) {
    if (x >= 0 && y >= 0 && x < cached->width && y < cached->height) {
        unsigned char* pixels = cairo_image_surface_get_data(cached->surface);
        if (cairo_image_surface_get_format(cached->surface) == CAIRO_FORMAT_ARGB32)
            return pixels + (y * (4 * cached->width)) + (4 * x);
    }
    return NULL;
}

VALUE shoes_image_get_pixel(VALUE self, VALUE _x, VALUE _y) {
    VALUE color = Qnil;
    int x = NUM2INT(_x), y = NUM2INT(_y);
    GET_STRUCT(image, image);
    unsigned char *pixels = shoes_image_surface_get_pixel(image->cached, x, y);
    if (pixels != NULL)
        color = shoes_color_new(pixels[2], pixels[1], pixels[0], pixels[3]);
    return color;
}

VALUE shoes_image_set_pixel(VALUE self, VALUE _x, VALUE _y, VALUE col) {
    int x = NUM2INT(_x), y = NUM2INT(_y);
    GET_STRUCT(image, image);
    shoes_image_ensure_dup(image);
    unsigned char *pixels = shoes_image_surface_get_pixel(image->cached, x, y);
    if (pixels != NULL) {
        if (TYPE(col) == T_STRING)
            col = shoes_color_parse(cColor, col);
        if (rb_obj_is_kind_of(col, cColor)) {
            shoes_color *color;
            Data_Get_Struct(col, shoes_color, color);
            pixels[0] = color->b;
            pixels[1] = color->g;
            pixels[2] = color->r;
            pixels[3] = color->a;
        }
    }
    return self;
}

VALUE shoes_image_get_path(VALUE self) {
    GET_STRUCT(image, image);
    return image->path;
}

VALUE shoes_image_set_path(VALUE self, VALUE path) {
    GET_STRUCT(image, image);
    image->path = path;
    image->cached = shoes_load_image(image->parent, path);
    image->type = SHOES_CACHE_FILE;
    shoes_canvas_repaint_all(image->parent);
    return path;
}

static void shoes_image_draw_surface(cairo_t *cr, shoes_image *self_t, shoes_place *place, cairo_surface_t *surf, int imw, int imh) {
    shoes_apply_transformation(cr, self_t->st, place, 0);
    cairo_translate(cr, place->ix + place->dx, place->iy + place->dy);
    if (place->iw != imw || place->ih != imh)
        cairo_scale(cr, (place->iw * 1.) / imw, (place->ih * 1.) / imh);
    cairo_set_source_surface(cr, surf, 0., 0.);
    cairo_paint(cr);
    shoes_undo_transformation(cr, self_t->st, place, 0);
    self_t->place = *place;
}

#define SHOES_IMAGE_PLACE(type, imw, imh, surf) \
  SETUP(shoes_##type, (REL_CANVAS | REL_SCALE), imw, imh); \
  VALUE ck = rb_obj_class(c); \
  if (RTEST(actual)) \
    shoes_image_draw_surface(CCR(canvas), self_t, &place, surf, imw, imh); \
  FINISH(); \
  return self;

VALUE shoes_image_draw(VALUE self, VALUE c, VALUE actual) {
    SHOES_IMAGE_PLACE(image, self_t->cached->width, self_t->cached->height, self_t->cached->surface);
}

void shoes_image_image(VALUE parent, VALUE path, VALUE attr) {
    shoes_image *pi;
    shoes_place place;
    Data_Get_Struct(parent, shoes_image, pi);
    VALUE self = shoes_image_new(cImage, path, attr, parent, pi->st);
    GET_STRUCT(image, image);
    shoes_image_ensure_dup(pi);
    shoes_place_exact(&place, image->attr, 0, 0);
    if (place.iw < 1) place.w = place.iw = image->cached->width;
    if (place.ih < 1) place.h = place.ih = image->cached->height;
    shoes_image_draw_surface(pi->cr, image, &place, image->cached->surface, image->cached->width, image->cached->height);
}

VALUE shoes_image_size(VALUE self) {
    GET_STRUCT(image, self_t);
    return rb_ary_new3(2, INT2NUM(self_t->cached->width), INT2NUM(self_t->cached->height));
}

VALUE shoes_image_motion(VALUE self, int x, int y, char *touch) {
    char h = 0;
    VALUE click;
    GET_STRUCT(image, self_t);

    click = ATTR(self_t->attr, click);
    if (self_t->cached == NULL) return Qnil;

    if (IS_INSIDE(self_t, x, y)) {
        if (!NIL_P(click)) {
            shoes_canvas *canvas;
            Data_Get_Struct(self_t->parent, shoes_canvas, canvas);
            shoes_app_cursor(canvas->app, s_link);
        }
        h = 1;
    }

    CHECK_HOVER(self_t, h, touch);

    return h ? click : Qnil;
}

VALUE shoes_image_send_click(VALUE self, int button, int x, int y) {
    VALUE v = Qnil;

    if (button > 0) {
        GET_STRUCT(image, self_t);
        v = shoes_image_motion(self, x, y, NULL);
        if (self_t->hover & HOVER_MOTION)
            self_t->hover = HOVER_MOTION | HOVER_CLICK;
    }

    return v;
}

void shoes_image_send_release(VALUE self, int button, int x, int y) {
    GET_STRUCT(image, self_t);
    if (button > 0 && (self_t->hover & HOVER_CLICK)) {
        VALUE proc = ATTR(self_t->attr, release);
        self_t->hover ^= HOVER_CLICK;
        if (!NIL_P(proc))
            shoes_safe_block(self, proc, rb_ary_new3(3, INT2NUM(button), INT2NUM(x), INT2NUM(y)));
    }
}

//
// Shoes::Pattern
//
void shoes_pattern_mark(shoes_pattern *pattern) {
    rb_gc_mark_maybe(pattern->source);
    rb_gc_mark_maybe(pattern->parent);
    rb_gc_mark_maybe(pattern->attr);
}

static void shoes_pattern_free(shoes_pattern *pattern) {
    if (pattern->pattern != NULL)
        cairo_pattern_destroy(pattern->pattern);
    RUBY_CRITICAL(free(pattern));
}

void shoes_pattern_gradient(shoes_pattern *pattern, VALUE r1, VALUE r2, VALUE attr) {
    double angle = ATTR2(dbl, attr, angle, 0.);
    double rads = angle * SHOES_RAD2PI;
    double dx = sin(rads);
    double dy = cos(rads);
    double edge = (fabs(dx) + fabs(dy)) * 0.5;

    if (rb_obj_is_kind_of(r1, rb_cString))
        r1 = shoes_color_parse(cColor, r1);
    if (rb_obj_is_kind_of(r2, rb_cString))
        r2 = shoes_color_parse(cColor, r2);

    VALUE radius = ATTR(attr, radius);
    if (!NIL_P(radius)) {
        double r = 0.001;
        if (rb_obj_is_kind_of(r, rb_cFloat)) r = NUM2DBL(radius);
        pattern->pattern = cairo_pattern_create_radial(0.5, 0.5, r, edge / 2., edge / 2., edge / 2.);
    } else {
        pattern->pattern = cairo_pattern_create_linear(0.5 + (-dx * edge), 0.5 + (-dy * edge),
                           0.5 + (dx * edge), 0.5 + (dy * edge));
    }
    shoes_color_grad_stop(pattern->pattern, 0.0, r1);
    shoes_color_grad_stop(pattern->pattern, 1.0, r2);
}

VALUE shoes_pattern_set_fill(VALUE self, VALUE source) {
    shoes_pattern *pattern;
    Data_Get_Struct(self, shoes_pattern, pattern);

    if (pattern->pattern != NULL)
        cairo_pattern_destroy(pattern->pattern);
    pattern->pattern = NULL;

    if (rb_obj_is_kind_of(source, rb_cRange)) {
        VALUE r1 = rb_funcall(source, s_begin, 0);
        VALUE r2 = rb_funcall(source, s_end, 0);
        shoes_pattern_gradient(pattern, r1, r2, pattern->attr);
    } else {
        if (!rb_obj_is_kind_of(source, cColor)) {
            VALUE rgb = shoes_color_parse(cColor, source);
            if (!NIL_P(rgb)) source = rgb;
        }

        if (rb_obj_is_kind_of(source, cColor)) {
            pattern->pattern = shoes_color_pattern(source);
        } else {
            pattern->cached = shoes_load_image(pattern->parent, source);
            if (pattern->cached != NULL && pattern->cached->pattern == NULL)
                pattern->cached->pattern = cairo_pattern_create_for_surface(pattern->cached->surface);
        }
        cairo_pattern_set_extend(PATTERN(pattern), CAIRO_EXTEND_REPEAT);
    }

    pattern->source = source;
    return source;
}

VALUE shoes_pattern_get_fill(VALUE self) {
    shoes_pattern *pattern;
    Data_Get_Struct(self, shoes_pattern, pattern);
    return pattern->source;
}

VALUE shoes_pattern_self(VALUE self) {
    return self;
}

VALUE shoes_pattern_args(int argc, VALUE *argv, VALUE self) {
    VALUE source, attr;
    rb_scan_args(argc, argv, "11", &source, &attr);
    CHECK_HASH(attr);
    return shoes_pattern_new(cPattern, source, attr, Qnil);
}

VALUE shoes_pattern_new(VALUE klass, VALUE source, VALUE attr, VALUE parent) {
    shoes_pattern *pattern;
    VALUE obj = shoes_pattern_alloc(klass);
    Data_Get_Struct(obj, shoes_pattern, pattern);
    pattern->source = Qnil;
    pattern->attr = attr;
    pattern->parent = parent;
    shoes_pattern_set_fill(obj, source);
    return obj;
}

VALUE shoes_pattern_method(VALUE klass, VALUE source) {
    return shoes_pattern_new(cPattern, source, Qnil, Qnil);
}

VALUE shoes_pattern_alloc(VALUE klass) {
    VALUE obj;
    shoes_pattern *pattern = SHOE_ALLOC(shoes_pattern);
    SHOE_MEMZERO(pattern, shoes_pattern, 1);
    obj = Data_Wrap_Struct(klass, shoes_pattern_mark, shoes_pattern_free, pattern);
    pattern->source = Qnil;
    pattern->attr = Qnil;
    pattern->parent = Qnil;
    return obj;
}

VALUE shoes_background_draw(VALUE self, VALUE c, VALUE actual) {
    cairo_matrix_t matrix1, matrix2;
    double r = 0., sw = 1.;
    SETUP(shoes_pattern, REL_TILE, PATTERN_DIM(self_t, width), PATTERN_DIM(self_t, height));
    r = ATTR2(dbl, self_t->attr, curve, 0.);

    if (RTEST(actual)) {
        cairo_t *cr = CCR(canvas);
        cairo_save(cr);
        cairo_translate(cr, place.ix + place.dx, place.iy + place.dy);
        PATTERN_SCALE(self_t, place, sw);
        cairo_new_path(cr);
        shoes_cairo_rect(cr, 0, 0, place.iw, place.ih, r);
        cairo_set_source(cr, PATTERN(self_t));
        cairo_fill(cr);
        cairo_restore(cr);
        PATTERN_RESET(self_t);
    }

    self_t->place = place;
    INFO("BACKGROUND: (%d, %d), (%d, %d)\n", place.x, place.y, place.w, place.h);
    return self;
}

VALUE shoes_border_draw(VALUE self, VALUE c, VALUE actual) {
    cairo_matrix_t matrix1, matrix2;
    ID cap = s_rect;
    ID dash = s_nodot;
    double r = 0., sw = 1.;
    SETUP(shoes_pattern, REL_TILE, PATTERN_DIM(self_t, width), PATTERN_DIM(self_t, height));
    r = ATTR2(dbl, self_t->attr, curve, 0.);
    sw = ATTR2(dbl, self_t->attr, strokewidth, 1.);
    if (!NIL_P(ATTR(self_t->attr, cap))) cap = SYM2ID(ATTR(self_t->attr, cap));
    if (!NIL_P(ATTR(self_t->attr, dash))) dash = SYM2ID(ATTR(self_t->attr, dash));

    place.iw -= (int)round(sw);
    place.ih -= (int)round(sw);
    place.ix += (int)round(sw / 2.);
    place.iy += (int)round(sw / 2.);

    if (RTEST(actual)) {
        cairo_t *cr = CCR(canvas);
        cairo_save(cr);
        cairo_translate(cr, place.ix + place.dx, place.iy + place.dy);
        PATTERN_SCALE(self_t, place, sw);
        cairo_set_source(cr, PATTERN(self_t));
        cairo_new_path(cr);
        shoes_cairo_rect(cr, 0, 0, place.iw, place.ih, r);
        cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
        CAP_SET(cr, cap);
        DASH_SET(cr, dash);
        cairo_set_line_width(cr, sw);
        cairo_stroke(cr);
        cairo_restore(cr);
        PATTERN_RESET(self_t);
    }

    self_t->place = place;
    INFO("BORDER: (%d, %d), (%d, %d)\n", place.x, place.y, place.w, place.h);
    return self;
}

VALUE shoes_subpattern_new(VALUE klass, VALUE pat, VALUE parent) {
    shoes_pattern *back, *pattern;
    VALUE obj = shoes_pattern_alloc(klass);
    Data_Get_Struct(obj, shoes_pattern, back);
    Data_Get_Struct(pat, shoes_pattern, pattern);
    back->source = pattern->source;
    back->cached = pattern->cached;
    back->pattern = pattern->pattern;
    if (back->pattern != NULL) cairo_pattern_reference(back->pattern);
    back->attr = pattern->attr;
    back->parent = parent;
    return obj;
}

VALUE shoes_app_method_missing(int argc, VALUE *argv, VALUE self) {
    VALUE cname, canvas;
    GET_STRUCT(app, app);

    cname = argv[0];
    canvas = rb_ary_entry(app->nesting, RARRAY_LEN(app->nesting) - 1);
    if (!NIL_P(canvas) && rb_respond_to(canvas, SYM2ID(cname)))
        return ts_funcall2(canvas, SYM2ID(cname), argc - 1, argv + 1);
    return shoes_color_method_missing(argc, argv, self);
}

//
// Shoes::TextBlock
//
void shoes_textblock_mark(shoes_textblock *text) {
    rb_gc_mark_maybe(text->texts);
    rb_gc_mark_maybe(text->links);
    rb_gc_mark_maybe(text->attr);
    rb_gc_mark_maybe(text->parent);
}

//
// Frees the pango attribute list.
// The `all` flag means the string has changed as well.
//
void shoes_textblock_uncache(shoes_textblock *text, unsigned char all) {
    if (text->pattr != NULL)
        pango_attr_list_unref(text->pattr);
    text->pattr = NULL;
    if (all) {
        if (text->text != NULL)
            g_string_free(text->text, TRUE);
        text->text = NULL;
        text->cached = 0;
    }
}

static void shoes_textblock_free(shoes_textblock *text) {
    shoes_transform_release(text->st);
    shoes_textblock_uncache(text, TRUE);
    if (text->cursor != NULL)
        SHOE_FREE(text->cursor);
    if (text->layout != NULL)
        g_object_unref(text->layout);
    RUBY_CRITICAL(free(text));
}

VALUE shoes_textblock_new(VALUE klass, VALUE texts, VALUE attr, VALUE parent, shoes_transform *st) {
    shoes_canvas *canvas;
    shoes_textblock *text;
    VALUE obj = shoes_textblock_alloc(klass);
    Data_Get_Struct(obj, shoes_textblock, text);
    Data_Get_Struct(parent, shoes_canvas, canvas);
    text->texts = shoes_text_check(texts, obj);
    text->attr = attr;
    text->parent = parent;
    text->st = shoes_transform_touch(st);
    return obj;
}

VALUE shoes_textblock_alloc(VALUE klass) {
    VALUE obj;
    shoes_textblock *text = SHOE_ALLOC(shoes_textblock);
    SHOE_MEMZERO(text, shoes_textblock, 1);
    obj = Data_Wrap_Struct(klass, shoes_textblock_mark, shoes_textblock_free, text);
    text->texts = Qnil;
    text->links = Qnil;
    text->attr = Qnil;
    text->parent = Qnil;
    text->cursor = NULL;
    return obj;
}

VALUE shoes_textblock_children(VALUE self) {
    GET_STRUCT(textblock, text);
    return text->texts;
}

static void shoes_textcursor_reset(shoes_textcursor *c) {
    c->pos = c->hi = c->x = c->y = INT_MAX;
}

VALUE shoes_textblock_set_cursor(VALUE self, VALUE pos) {
    GET_STRUCT(textblock, self_t);
    if (self_t->cursor == NULL) {
        if (NIL_P(pos)) return Qnil;
        else            shoes_textcursor_reset(self_t->cursor = SHOE_ALLOC(shoes_textcursor));
    }

    if (NIL_P(pos)) shoes_textcursor_reset(self_t->cursor);
    else if (pos == ID2SYM(s_marker)) {
        if (self_t->cursor->hi != INT_MAX) {
            self_t->cursor->pos = min(self_t->cursor->pos, self_t->cursor->hi);
            self_t->cursor->hi = INT_MAX;
        }
    } else            self_t->cursor->pos = NUM2INT(pos);
    shoes_textblock_uncache(self_t, FALSE);
    shoes_canvas_repaint_all(self_t->parent);
    return pos;
}

VALUE shoes_textblock_get_cursor(VALUE self) {
    GET_STRUCT(textblock, self_t);
    if (self_t->cursor == NULL || self_t->cursor->pos == INT_MAX) return Qnil;
    return INT2NUM(self_t->cursor->pos);
}

VALUE shoes_textblock_cursorx(VALUE self) {
    GET_STRUCT(textblock, self_t);
    if (self_t->cursor == NULL || self_t->cursor->x == INT_MAX) return Qnil;
    return INT2NUM(self_t->cursor->x);
}

VALUE shoes_textblock_cursory(VALUE self) {
    GET_STRUCT(textblock, self_t);
    if (self_t->cursor == NULL || self_t->cursor->y == INT_MAX) return Qnil;
    return INT2NUM(self_t->cursor->y);
}

VALUE shoes_textblock_set_marker(VALUE self, VALUE pos) {
    GET_STRUCT(textblock, self_t);
    if (self_t->cursor == NULL) {
        if (NIL_P(pos)) return Qnil;
        else            shoes_textcursor_reset(self_t->cursor = SHOE_ALLOC(shoes_textcursor));
    }

    if (NIL_P(pos)) self_t->cursor->hi = INT_MAX;
    else            self_t->cursor->hi = NUM2INT(pos);
    shoes_textblock_uncache(self_t, FALSE);
    shoes_canvas_repaint_all(self_t->parent);
    return pos;
}

VALUE shoes_textblock_get_marker(VALUE self) {
    GET_STRUCT(textblock, self_t);
    if (self_t->cursor == NULL || self_t->cursor->hi == INT_MAX) return Qnil;
    return INT2NUM(self_t->cursor->hi);
}

VALUE shoes_textblock_get_highlight(VALUE self) {
    int marker, start, len;
    GET_STRUCT(textblock, self_t);
    if (self_t->cursor == NULL || self_t->cursor->pos == INT_MAX) return Qnil;
    marker = self_t->cursor->hi;
    if (marker == INT_MAX) marker = self_t->cursor->pos;
    start = min(self_t->cursor->pos, marker);
    len = max(self_t->cursor->pos, marker) - start;
    return rb_ary_new3(2, INT2NUM(start), INT2NUM(len));
}

VALUE shoes_find_textblock(VALUE self) {
    while (!NIL_P(self) && !rb_obj_is_kind_of(self, cTextBlock)) {
        SETUP_BASIC();
        self = basic->parent;
    }
    return self;
}

static VALUE shoes_textblock_send_hover(VALUE self, int x, int y, VALUE *clicked, char *t) {
    VALUE url = Qnil;
    int index, trailing, i, hover;
    GET_STRUCT(textblock, self_t);
    if (self_t->layout == NULL || NIL_P(self_t->links)) return Qnil;
    if (!NIL_P(self_t->attr) && ATTR(self_t->attr, hidden) == Qtrue) return Qnil;

    x -= self_t->place.ix + self_t->place.dx;
    y -= self_t->place.iy + self_t->place.dy;
    hover = pango_layout_xy_to_index(self_t->layout, x * PANGO_SCALE, y * PANGO_SCALE, &index, &trailing);
    if (hover != (self_t->hover & HOVER_MOTION)) {
        shoes_textblock_uncache(self_t, FALSE);
        INFO("HOVER (%d, %d) OVER (%d, %d)\n", x, y, self_t->place.ix + self_t->place.dx, self_t->place.iy + self_t->place.dy);
    }
    for (i = 0; i < RARRAY_LEN(self_t->links); i++) {
        VALUE urll = shoes_link_at(self_t, rb_ary_entry(self_t->links, i), index, hover, clicked, t);
        if (NIL_P(url)) url = urll;
    }

    return url;
}

VALUE shoes_textblock_motion(VALUE self, int x, int y, char *t) {
    VALUE url = shoes_textblock_send_hover(self, x, y, NULL, t);
    if (!NIL_P(url)) {
        shoes_canvas *canvas;
        GET_STRUCT(textblock, self_t);
        Data_Get_Struct(self_t->parent, shoes_canvas, canvas);
        shoes_app_cursor(canvas->app, s_link);
    }
    return url;
}

VALUE shoes_textblock_hit(VALUE self, VALUE _x, VALUE _y) {
    int x = NUM2INT(_x), y = NUM2INT(_y), index, trailing;
    GET_STRUCT(textblock, self_t);
    x -= self_t->place.ix + self_t->place.dx;
    y -= self_t->place.iy + self_t->place.dy;
    if (x < 0 || x > self_t->place.iw || y < 0 || y > self_t->place.ih)
        return Qnil;
    pango_layout_xy_to_index(self_t->layout, x * PANGO_SCALE, y * PANGO_SCALE, &index, &trailing);
    return INT2NUM(index);
}

VALUE shoes_textblock_send_click(VALUE self, int button, int x, int y, VALUE *clicked) {
    VALUE v = Qnil;

    if (button > 0) {
        GET_STRUCT(textblock, self_t);
        v = shoes_textblock_send_hover(self, x, y, clicked, NULL);
        if (self_t->hover & HOVER_MOTION)
            self_t->hover = HOVER_MOTION | HOVER_CLICK;
    }

    return v;
}

void shoes_textblock_send_release(VALUE self, int button, int x, int y) {
    GET_STRUCT(textblock, self_t);
    if (button > 0 && (self_t->hover & HOVER_CLICK)) {
        VALUE proc = ATTR(self_t->attr, release);
        self_t->hover ^= HOVER_CLICK;
        if (!NIL_P(proc))
            shoes_safe_block(self, proc, rb_ary_new3(1, self));
    }
}

#define APPLY_ATTR() \
  if (attr != NULL) { \
    attr->start_index = start_index; \
    attr->end_index = end_index; \
    pango_attr_list_insert_before(block->pattr, attr); \
    attr = NULL; \
  }

#define GET_STYLE(name) \
  attr = NULL; \
  str = Qnil; \
  if (!NIL_P(oattr)) str = rb_hash_aref(oattr, ID2SYM(s_##name)); \
  if (!NIL_P(hsh) && NIL_P(str)) str = rb_hash_aref(hsh, ID2SYM(s_##name))

#define APPLY_STYLE_COLOR(name, func) \
  GET_STYLE(name); \
  if (!NIL_P(str)) \
  { \
    if (TYPE(str) == T_STRING) \
      str = shoes_color_parse(cColor, str); \
    if (rb_obj_is_kind_of(str, cColor)) \
    { \
      shoes_color *color; \
      Data_Get_Struct(str, shoes_color, color); \
      attr = pango_attr_##func##_new(color->r * 255, color->g * 255, color-> b * 255); \
    } \
    APPLY_ATTR(); \
  }

static void shoes_app_style_for(shoes_textblock *block, shoes_app *app, VALUE klass, VALUE oattr, guint start_index, guint end_index) {
    VALUE str = Qnil;
    VALUE hsh = rb_hash_aref(app->styles, klass);
    if (NIL_P(hsh) && NIL_P(oattr)) return;

    PangoAttribute *attr = NULL;

    APPLY_STYLE_COLOR(stroke, foreground);
    APPLY_STYLE_COLOR(fill, background);
    APPLY_STYLE_COLOR(strikecolor, strikethrough_color);
    APPLY_STYLE_COLOR(undercolor, underline_color);

    GET_STYLE(font);
    if (!NIL_P(str)) {
        if (TYPE(str) == T_STRING) {
            attr = pango_attr_font_desc_new(pango_font_description_from_string(RSTRING_PTR(str)));
        }
        APPLY_ATTR();
    }

    GET_STYLE(size);
    if (!NIL_P(str)) {
        if (TYPE(str) == T_STRING) {
            if (strncmp(RSTRING_PTR(str), "xx-small", 8) == 0)
                attr = pango_attr_scale_new(PANGO_SCALE_XX_SMALL);
            else if (strncmp(RSTRING_PTR(str), "x-small", 7) == 0)
                attr = pango_attr_scale_new(PANGO_SCALE_X_SMALL);
            else if (strncmp(RSTRING_PTR(str), "small", 5) == 0)
                attr = pango_attr_scale_new(PANGO_SCALE_SMALL);
            else if (strncmp(RSTRING_PTR(str), "medium", 6) == 0)
                attr = pango_attr_scale_new(PANGO_SCALE_MEDIUM);
            else if (strncmp(RSTRING_PTR(str), "large", 5) == 0)
                attr = pango_attr_scale_new(PANGO_SCALE_LARGE);
            else if (strncmp(RSTRING_PTR(str), "x-large", 7) == 0)
                attr = pango_attr_scale_new(PANGO_SCALE_X_LARGE);
            else if (strncmp(RSTRING_PTR(str), "xx-large", 8) == 0)
                attr = pango_attr_scale_new(PANGO_SCALE_XX_LARGE);
            else
                str = rb_funcall(str, s_to_i, 0);
        }
        if (TYPE(str) == T_FIXNUM) {
            int i = NUM2INT(str);
            if (i > 0)
                attr = pango_attr_size_new_absolute(ROUND(i * PANGO_SCALE * (96./72.)));
        }
        APPLY_ATTR();
    }

    GET_STYLE(family);
    if (!NIL_P(str)) {
        if (TYPE(str) == T_STRING) {
            attr = pango_attr_family_new(RSTRING_PTR(str));
        }
        APPLY_ATTR();
    }

    GET_STYLE(weight);
    if (!NIL_P(str)) {
        if (TYPE(str) == T_STRING) {
            if (strncmp(RSTRING_PTR(str), "ultralight", 10) == 0)
                attr = pango_attr_weight_new(PANGO_WEIGHT_ULTRALIGHT);
            else if (strncmp(RSTRING_PTR(str), "light", 5) == 0)
                attr = pango_attr_weight_new(PANGO_WEIGHT_LIGHT);
            else if (strncmp(RSTRING_PTR(str), "normal", 6) == 0)
                attr = pango_attr_weight_new(PANGO_WEIGHT_NORMAL);
            else if (strncmp(RSTRING_PTR(str), "semibold", 8) == 0)
                attr = pango_attr_weight_new(PANGO_WEIGHT_SEMIBOLD);
            else if (strncmp(RSTRING_PTR(str), "bold", 4) == 0)
                attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
            else if (strncmp(RSTRING_PTR(str), "ultrabold", 9) == 0)
                attr = pango_attr_weight_new(PANGO_WEIGHT_ULTRABOLD);
            else if (strncmp(RSTRING_PTR(str), "heavy", 5) == 0)
                attr = pango_attr_weight_new(PANGO_WEIGHT_HEAVY);
        } else if (TYPE(str) == T_FIXNUM) {
            int i = NUM2INT(str);
            if (i >= 100 && i <= 900)
                attr = pango_attr_weight_new((PangoWeight)i);
        }
        APPLY_ATTR();
    }

    GET_STYLE(rise);
    if (!NIL_P(str)) {
        if (TYPE(str) == T_STRING)
            str = rb_funcall(str, s_to_i, 0);
        if (TYPE(str) == T_FIXNUM) {
            int i = NUM2INT(str);
            attr = pango_attr_rise_new(i * PANGO_SCALE);
        }
        APPLY_ATTR();
    }

    GET_STYLE(kerning);
    if (!NIL_P(str)) {
        if (TYPE(str) == T_STRING)
            str = rb_funcall(str, s_to_i, 0);
        if (TYPE(str) == T_FIXNUM) {
            int i = NUM2INT(str);
            attr = pango_attr_letter_spacing_new(i * PANGO_SCALE);
        }
        APPLY_ATTR();
    }

    GET_STYLE(emphasis);
    if (!NIL_P(str)) {
        if (TYPE(str) == T_STRING) {
            if (strncmp(RSTRING_PTR(str), "normal", 6) == 0)
                attr = pango_attr_style_new(PANGO_STYLE_NORMAL);
            else if (strncmp(RSTRING_PTR(str), "oblique", 7) == 0)
                attr = pango_attr_style_new(PANGO_STYLE_OBLIQUE);
            else if (strncmp(RSTRING_PTR(str), "italic", 6) == 0)
                attr = pango_attr_style_new(PANGO_STYLE_ITALIC);
        }
        APPLY_ATTR();
    }

    GET_STYLE(strikethrough);
    if (!NIL_P(str)) {
        if (TYPE(str) == T_STRING) {
            if (strncmp(RSTRING_PTR(str), "none", 4) == 0)
                attr = pango_attr_strikethrough_new(FALSE);
            else if (strncmp(RSTRING_PTR(str), "single", 6) == 0)
                attr = pango_attr_strikethrough_new(TRUE);
        }
        APPLY_ATTR();
    }

    GET_STYLE(stretch);
    if (!NIL_P(str)) {
        if (TYPE(str) == T_STRING) {
            if (strncmp(RSTRING_PTR(str), "condensed", 9) == 0)
                attr = pango_attr_stretch_new(PANGO_STRETCH_CONDENSED);
            else if (strncmp(RSTRING_PTR(str), "normal", 6) == 0)
                attr = pango_attr_stretch_new(PANGO_STRETCH_NORMAL);
            else if (strncmp(RSTRING_PTR(str), "expanded", 8) == 0)
                attr = pango_attr_stretch_new(PANGO_STRETCH_EXPANDED);
        }
        APPLY_ATTR();
    }

    GET_STYLE(underline);
    if (!NIL_P(str)) {
        if (TYPE(str) == T_STRING) {
            if (strncmp(RSTRING_PTR(str), "none", 4) == 0)
                attr = pango_attr_underline_new(PANGO_UNDERLINE_NONE);
            else if (strncmp(RSTRING_PTR(str), "single", 6) == 0)
                attr = pango_attr_underline_new(PANGO_UNDERLINE_SINGLE);
            else if (strncmp(RSTRING_PTR(str), "double", 6) == 0)
                attr = pango_attr_underline_new(PANGO_UNDERLINE_DOUBLE);
            else if (strncmp(RSTRING_PTR(str), "low", 3) == 0)
                attr = pango_attr_underline_new(PANGO_UNDERLINE_LOW);
            else if (strncmp(RSTRING_PTR(str), "error", 5) == 0)
                attr = pango_attr_underline_new(PANGO_UNDERLINE_ERROR);
        }
        APPLY_ATTR();
    }

    GET_STYLE(variant);
    if (!NIL_P(str)) {
        if (TYPE(str) == T_STRING) {
            if (strncmp(RSTRING_PTR(str), "normal", 6) == 0)
                attr = pango_attr_variant_new(PANGO_VARIANT_NORMAL);
            else if (strncmp(RSTRING_PTR(str), "smallcaps", 9) == 0)
                attr = pango_attr_variant_new(PANGO_VARIANT_SMALL_CAPS);
        }
        APPLY_ATTR();
    }
}

static void shoes_textblock_iter_pango(VALUE texts, shoes_textblock *block, shoes_app *app) {
    VALUE v;
    long i;

    if (NIL_P(texts))
        return;

    for (i = 0; i < RARRAY_LEN(texts); i++) {
        v = rb_ary_entry(texts, i);
        if (rb_obj_is_kind_of(v, cTextClass)) {
            VALUE tklass = rb_obj_class(v);
            guint start;
            shoes_text *text;
            Data_Get_Struct(v, shoes_text, text);

            start = block->len;
            shoes_textblock_iter_pango(text->texts, block, app);
            if ((text->hover & HOVER_MOTION) && tklass == cLink)
                tklass = cLinkHover;
            shoes_app_style_for(block, app, tklass, text->attr, start, block->len);
            if (!block->cached && rb_obj_is_kind_of(v, cLink) && !NIL_P(text->attr)) {
                rb_ary_push(block->links, shoes_link_new(v, start, block->len));
            }
        } else if (rb_obj_is_kind_of(v, rb_cArray)) {
            shoes_textblock_iter_pango(v, block, app);
        } else {
            char *start, *end;
            v = rb_funcall(v, s_to_s, 0);
            block->len += (guint)RSTRING_LEN(v);
            if (!block->cached) {
                start = RSTRING_PTR(v);
                if (!g_utf8_validate(start, RSTRING_LEN(v), (const gchar **)&end))
                    shoes_error("not a valid UTF-8 string: %.*s", end - start, start);
                if (end > start)
                    g_string_append_len(block->text, start, end - start);
            }
        }
    }
}

static void shoes_textblock_make_pango(shoes_app *app, VALUE klass, shoes_textblock *block) {
    if (!block->cached) {
        block->text = g_string_new(NULL);
        block->links = rb_ary_new();
    }
    block->len = 0;
    block->pattr = pango_attr_list_new();

    shoes_textblock_iter_pango(block->texts, block, app);
    shoes_app_style_for(block, app, klass, block->attr, 0, block->len);

    if (block->cursor != NULL && block->cursor->pos != INT_MAX &&
            block->cursor->hi != INT_MAX && block->cursor->pos != block->cursor->hi) {
        PangoAttribute *attr = pango_attr_background_new(255 * 255, 255 * 255, 0);
        attr->start_index = min(block->cursor->pos, block->cursor->hi);
        attr->end_index = max(block->cursor->pos, block->cursor->hi);
        pango_attr_list_insert(block->pattr, attr);
    }

    block->cached = 1;
}

static void shoes_textblock_on_layout(shoes_app *app, VALUE klass, shoes_textblock *block) {
    char *attr = NULL;
    VALUE str = Qnil, hsh = Qnil, oattr = Qnil;
    g_return_if_fail(block != NULL);
    g_return_if_fail(PANGO_IS_LAYOUT(block->layout));

    oattr = block->attr;
    hsh = rb_hash_aref(app->styles, klass);

    if (!block->cached || block->pattr == NULL)
        shoes_textblock_make_pango(app, klass, block);
    pango_layout_set_text(block->layout, block->text->str, -1);
    pango_layout_set_attributes(block->layout, block->pattr);

    GET_STYLE(justify);
    if (!NIL_P(str))
        pango_layout_set_justify(block->layout, RTEST(str));

    GET_STYLE(align);
    if (TYPE(str) == T_STRING) {
        if (strncmp(RSTRING_PTR(str), "left", 4) == 0)
            pango_layout_set_alignment(block->layout, PANGO_ALIGN_LEFT);
        else if (strncmp(RSTRING_PTR(str), "center", 6) == 0)
            pango_layout_set_alignment(block->layout, PANGO_ALIGN_CENTER);
        else if (strncmp(RSTRING_PTR(str), "right", 5) == 0)
            pango_layout_set_alignment(block->layout, PANGO_ALIGN_RIGHT);
    }

    GET_STYLE(wrap);
    if (TYPE(str) == T_STRING) {
        if (strncmp(RSTRING_PTR(str), "word", 4) == 0)
            pango_layout_set_wrap(block->layout, PANGO_WRAP_WORD);
        else if (strncmp(RSTRING_PTR(str), "char", 4) == 0)
            pango_layout_set_wrap(block->layout, PANGO_WRAP_CHAR);
        else if (strncmp(RSTRING_PTR(str), "trim", 4) == 0)
            pango_layout_set_ellipsize(block->layout, PANGO_ELLIPSIZE_END);
    }
}

VALUE shoes_textblock_draw(VALUE self, VALUE c, VALUE actual) {
    double crx = 0., cry = 0.;
    int px, py, pd, li, ld;
    cairo_t *cr;
    shoes_canvas *canvas;
    PangoLayoutLine *last;
    PangoRectangle crect, lrect;

    VALUE ck = rb_obj_class(c);
    GET_STRUCT(textblock, self_t);
    Data_Get_Struct(c, shoes_canvas, canvas);
    cr = CCR(canvas);

    if (!NIL_P(self_t->attr) && ATTR(self_t->attr, hidden) == Qtrue)
        return self;

    ATTR_MARGINS(self_t->attr, 4, canvas);
    if (NIL_P(ATTR(self_t->attr, margin)) && NIL_P(ATTR(self_t->attr, margin_bottom)))
        bmargin = 12;
    self_t->place.flags = REL_CANVAS;
    self_t->place.flags |= NIL_P(ATTR(self_t->attr, left)) && NIL_P(ATTR(self_t->attr, right)) ? 0 : FLAG_ABSX;
    self_t->place.flags |= NIL_P(ATTR(self_t->attr, top)) && NIL_P(ATTR(self_t->attr, bottom)) ? 0 : FLAG_ABSY;
    self_t->place.x = ATTR2(int, self_t->attr, left, canvas->cx);
    self_t->place.y = ATTR2(int, self_t->attr, top, canvas->cy);
    if (!ORIGIN(canvas->place)) {
        self_t->place.dx = canvas->place.dx;
        self_t->place.dy = canvas->place.dy;
    } else {
        self_t->place.dx = 0;
        self_t->place.dy = 0;
    }
    self_t->place.dx += PXN(self_t->attr, displace_left, 0, CPW(canvas));
    self_t->place.dy += PXN(self_t->attr, displace_top, 0, CPH(canvas));
    self_t->place.w = ATTR2(int, self_t->attr, width, canvas->place.iw - (canvas->cx - self_t->place.x));
    self_t->place.iw = self_t->place.w - (lmargin + rmargin);
    ld = ATTR2(int, self_t->attr, leading, 4);

    if (self_t->layout != NULL)
        g_object_unref(self_t->layout);

    self_t->layout = pango_cairo_create_layout(cr);
    pd = 0;
    if (!ABSX(self_t->place) && self_t->place.x == canvas->cx) {
        if (self_t->place.x - CPX(canvas) > self_t->place.w) {
            self_t->place.x = CPX(canvas);
            canvas->cy = self_t->place.y = canvas->endy;
        } else {
            if (self_t->place.x > CPX(canvas)) {
                pd = self_t->place.x - CPX(canvas);
                pango_layout_set_indent(self_t->layout, pd * PANGO_SCALE);
                self_t->place.x = CPX(canvas);
            }
        }
    }

    pango_layout_set_width(self_t->layout, self_t->place.iw * PANGO_SCALE);
    pango_layout_set_spacing(self_t->layout, ld * PANGO_SCALE);
    shoes_textblock_on_layout(canvas->app, rb_obj_class(self), self_t);
    pango_layout_set_font_description(self_t->layout, shoes_world->default_font);

    //
    // Line up the first line with the y-cursor
    //
    if (!ABSX(self_t->place) && !ABSY(self_t->place) && pd) {
        last = pango_layout_get_line(self_t->layout, 0);
        pango_layout_line_get_pixel_extents(last, NULL, &lrect);
        if (lrect.width > self_t->place.iw - pd) {
            pango_layout_set_indent(self_t->layout, 0);
            self_t->place.x = CPX(canvas);
            canvas->cy = self_t->place.y = canvas->endy;
            pd = 0;
        }
    }
    self_t->place.ix = self_t->place.x + lmargin;
    self_t->place.iy = self_t->place.y + tmargin;

    li = pango_layout_get_line_count(self_t->layout) - 1;
    last = pango_layout_get_line(self_t->layout, li);
    pango_layout_line_get_pixel_extents(last, NULL, &lrect);
    pango_layout_get_pixel_size(self_t->layout, &px, &py);

    if (self_t->cursor != NULL && self_t->cursor->pos != INT_MAX) {
        int cursor = self_t->cursor->pos;
        if (cursor < 0) cursor += self_t->text->len + 1;
        pango_layout_index_to_pos(self_t->layout, cursor, &crect);
        crx = (self_t->place.ix + self_t->place.dx) + (crect.x / PANGO_SCALE);
        cry = (self_t->place.iy + self_t->place.dy) + (crect.y / PANGO_SCALE);
        self_t->cursor->x = (int)crx;
        self_t->cursor->y = (int)cry;
    }

    if (RTEST(actual)) {
        shoes_apply_transformation(cr, self_t->st, &self_t->place, 0);
        if (shoes_shape_check(cr, &self_t->place)) {
            cairo_move_to(cr, self_t->place.ix + self_t->place.dx, self_t->place.iy + self_t->place.dy);
            cairo_set_source_rgb(cr, 0., 0., 0.);
            pango_cairo_update_layout(cr, self_t->layout);
            pango_cairo_show_layout(cr, self_t->layout);

            if (self_t->cursor != NULL && self_t->cursor->pos != INT_MAX) {
                cairo_save(cr);
                cairo_new_path(cr);
                cairo_move_to(cr, crx, cry);
                cairo_line_to(cr, crx, cry + (crect.height / PANGO_SCALE));
                cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
                cairo_set_source_rgb(cr, 0., 0., 0.);
                cairo_set_line_width(cr, 1.);
                cairo_stroke(cr);
                cairo_restore(cr);
            }
        }

        shoes_undo_transformation(cr, self_t->st, &self_t->place, 0);
    }

    if (li == 0) {
        self_t->place.iw = px;
        self_t->place.w = px + lmargin + rmargin;
    }
    self_t->place.ih = py;
    self_t->place.h = py + tmargin + bmargin;
    INFO("TEXT: %d, %d (%d, %d) / (%d: %d, %d: %d) %d, %d [%d]\n", canvas->cx, canvas->cy,
         canvas->place.w, canvas->height, self_t->place.x, self_t->place.ix,
         self_t->place.y, self_t->place.iy, self_t->place.w, self_t->place.h, pd);

    if (!ABSY(self_t->place)) {
        // newlines have an empty size
        if (ck != cStack) {
            if (li == 0) {
                canvas->cx = self_t->place.x + lrect.x + lrect.width + rmargin + pd;
            } else {
                canvas->cy = self_t->place.y + py - lrect.height;
                if (lrect.width == 0) {
                    canvas->cx = self_t->place.x + lrect.x;
                } else {
                    canvas->cx = self_t->place.x + lrect.width + rmargin;
                }
            }
        }

        canvas->endy = max(self_t->place.y + self_t->place.h, canvas->endy);
        canvas->endx = canvas->cx;

        if (ck == cStack || canvas->cx - CPX(canvas) > canvas->width) {
            canvas->cx = CPX(canvas);
            canvas->cy = canvas->endy;
        }
        if (NIL_P(ATTR(self_t->attr, margin)) && NIL_P(ATTR(self_t->attr, margin_top)))
            bmargin = lrect.height;

        INFO("CX: (%d, %d) / LRECT: (%d, %d) / END: (%d, %d)\n",
             canvas->cx, canvas->cy,
             lrect.x, lrect.width,
             canvas->endx, canvas->endy);
    }
    return self;
}

VALUE shoes_textblock_string(VALUE self) {
    shoes_canvas *canvas;
    GET_STRUCT(textblock, self_t);
    Data_Get_Struct(self_t->parent, shoes_canvas, canvas);
    if (!self_t->cached || self_t->pattr == NULL)
        shoes_textblock_make_pango(canvas->app, rb_obj_class(self), self_t);
    return rb_str_new(self_t->text->str, self_t->text->len);
}

PLACE_COMMON(canvas)
TRANS_COMMON(canvas, 0);
PLACE_COMMON(image)
CLASS_COMMON2(image)
TRANS_COMMON(image, 1);
CLASS_COMMON2(pattern)
PLACE_COMMON(textblock)
CLASS_COMMON2(textblock)
REPLACE_COMMON(textblock)

VALUE shoes_textblock_style_m(int argc, VALUE *argv, VALUE self) {
    GET_STRUCT(textblock, self_t);
    VALUE obj = shoes_textblock_style(argc, argv, self);
    shoes_textblock_uncache(self_t, FALSE);
    return obj;
}

void shoes_msg(ID typ, VALUE str) {
#ifndef RUBY_1_8
    ID func = rb_frame_this_func();
    rb_ary_push(shoes_world->msgs, rb_ary_new3(6,
                ID2SYM(typ), str, rb_funcall(rb_cTime, s_now, 0),
                func ? ID2SYM(func) : Qnil,
                rb_str_new2("<unknown>"), INT2NUM(0)));
#else
    ID func = rb_frame_last_func();
    rb_ary_push(shoes_world->msgs, rb_ary_new3(6,
                ID2SYM(typ), str, rb_funcall(rb_cTime, s_now, 0),
                func ? ID2SYM(func) : Qnil,
                rb_str_new2(ruby_sourcefile), INT2NUM(ruby_sourceline)));
#endif
}

#define DEBUG_TYPE(t) \
  VALUE \
  shoes_canvas_##t(VALUE self, VALUE str) \
  { \
    shoes_msg(s_##t, str); \
    return Qnil; \
  } \
  \
  void \
  shoes_##t(const char *fmt, ...) \
  { \
    va_list args; \
    char buf[BUFSIZ]; \
  \
    va_start(args,fmt); \
    vsnprintf(buf, BUFSIZ, fmt, args); \
    va_end(args); \
    shoes_msg(s_##t, rb_str_new2(buf)); \
  }

DEBUG_TYPE(info);
DEBUG_TYPE(debug);
DEBUG_TYPE(warn);
DEBUG_TYPE(error);

VALUE shoes_p(VALUE self, VALUE obj) {
    return shoes_canvas_debug(self, rb_inspect(obj));
}

VALUE shoes_log(VALUE self) {
    return shoes_world->msgs;
}

VALUE shoes_font(VALUE self, VALUE path) {
    StringValue(path);
    return shoes_load_font(RSTRING_PTR(path));
}

//
// See ruby.h for the complete list of App methods which redirect to Canvas.
//
CANVAS_DEFS(FUNC_M);

#define C(n, s) \
  re##n = rb_eval_string(s); \
  rb_const_set(cShoes, rb_intern("" # n), re##n);

VALUE progname;

//
// Everything exposed to Ruby is exposed here.
//
void shoes_ruby_init() {
    progname = rb_str_new2("(eval)");
    rb_define_variable("$0", &progname);
    rb_define_variable("$PROGRAM_NAME", &progname);

    instance_eval_proc = rb_eval_string("lambda{|o,b| o.instance_eval(&b)}");
    rb_gc_register_address(&instance_eval_proc);
    ssNestSlot = rb_eval_string("{:height => 1.0}");
    rb_gc_register_address(&ssNestSlot);

    s_aref = rb_intern("[]=");
    s_perc = rb_intern("%");
    s_mult = rb_intern("*");
    SYMBOL_DEFS(SYMBOL_INTERN);

    symAltQuest = ID2SYM(rb_intern("alt_?"));
    symAltSlash = ID2SYM(rb_intern("alt_/"));
    symAltEqual = ID2SYM(rb_intern("alt_="));
    symAltDot = ID2SYM(rb_intern("alt_."));
    symAltSemiColon = ID2SYM(rb_intern("alt_;"));

    //
    // I want all elements to be addressed Shoes::Name, but also available in
    // a separate mixin (cTypes), for inclusion in every Shoes.app block.
    //
    cTypes = rb_define_module("Shoes");
    rb_mod_remove_const(rb_cObject, rb_str_new2("Shoes"));

    cShoesWindow = rb_define_class_under(cTypes, "Window", rb_cObject);
    cMouse = rb_define_class_under(cTypes, "Mouse", rb_cObject);

    cCanvas = rb_define_class_under(cTypes, "Canvas", rb_cObject);
    rb_define_alloc_func(cCanvas, shoes_canvas_alloc);
    rb_define_method(cCanvas, "top", CASTHOOK(shoes_canvas_get_top), 0);
    rb_define_method(cCanvas, "left", CASTHOOK(shoes_canvas_get_left), 0);
    rb_define_method(cCanvas, "width", CASTHOOK(shoes_canvas_get_width), 0);
    rb_define_method(cCanvas, "height", CASTHOOK(shoes_canvas_get_height), 0);
    rb_define_method(cCanvas, "scroll_height", CASTHOOK(shoes_canvas_get_scroll_height), 0);
    rb_define_method(cCanvas, "scroll_max", CASTHOOK(shoes_canvas_get_scroll_max), 0);
    rb_define_method(cCanvas, "scroll_top", CASTHOOK(shoes_canvas_get_scroll_top), 0);
    rb_define_method(cCanvas, "scroll_top=", CASTHOOK(shoes_canvas_set_scroll_top), 1);
    rb_define_method(cCanvas, "displace", CASTHOOK(shoes_canvas_displace), 2);
    rb_define_method(cCanvas, "move", CASTHOOK(shoes_canvas_move), 2);
    rb_define_method(cCanvas, "style", CASTHOOK(shoes_canvas_style), -1);
    rb_define_method(cCanvas, "parent", CASTHOOK(shoes_canvas_get_parent), 0);
    rb_define_method(cCanvas, "contents", CASTHOOK(shoes_canvas_contents), 0);
    rb_define_method(cCanvas, "children", CASTHOOK(shoes_canvas_children), 0);
    rb_define_method(cCanvas, "draw", CASTHOOK(shoes_canvas_draw), 2);
    rb_define_method(cCanvas, "hide", CASTHOOK(shoes_canvas_hide), 0);
    rb_define_method(cCanvas, "show", CASTHOOK(shoes_canvas_show), 0);
    rb_define_method(cCanvas, "toggle", CASTHOOK(shoes_canvas_toggle), 0);
    rb_define_method(cCanvas, "remove", CASTHOOK(shoes_canvas_remove), 0);
    rb_define_method(cCanvas, "refresh_slot", CASTHOOK(shoes_canvas_refresh_slot), 0);
    rb_define_method(cCanvas, "refresh", CASTHOOK(shoes_canvas_refresh_slot), 0);
    rb_define_method(cCanvas, "cursor=", CASTHOOK(shoes_canvas_get_cursor), 0);
    rb_define_method(cCanvas, "cursor=", CASTHOOK(shoes_canvas_set_cursor), 1);

    cShoes = rb_define_class("Shoes", cCanvas);
    rb_include_module(cShoes, cTypes);
    rb_const_set(cShoes, rb_intern("Types"), cTypes);

    shoes_version_init();

    // other Shoes:: constants
    rb_const_set(cTypes, rb_intern("RAD2PI"), rb_float_new(SHOES_RAD2PI));
    rb_const_set(cTypes, rb_intern("TWO_PI"), rb_float_new(SHOES_PIM2));
    rb_const_set(cTypes, rb_intern("HALF_PI"), rb_float_new(SHOES_HALFPI));
    rb_const_set(cTypes, rb_intern("PI"), rb_float_new(SHOES_PI));

    cApp = rb_define_class_under(cTypes, "App", rb_cObject);
    rb_define_alloc_func(cApp, shoes_app_alloc);
    rb_define_method(cApp, "fullscreen", CASTHOOK(shoes_app_get_fullscreen), 0);
    rb_define_method(cApp, "fullscreen=", CASTHOOK(shoes_app_set_fullscreen), 1);
    rb_define_method(cApp, "name", CASTHOOK(shoes_app_get_title), 0);
    rb_define_method(cApp, "name=", CASTHOOK(shoes_app_set_title), 1);
    rb_define_method(cApp, "location", CASTHOOK(shoes_app_location), 0);
    rb_define_method(cApp, "started?", CASTHOOK(shoes_app_is_started), 0);
    rb_define_method(cApp, "width", CASTHOOK(shoes_app_get_width), 0);
    rb_define_method(cApp, "height", CASTHOOK(shoes_app_get_height), 0);
    rb_define_method(cApp, "slot", CASTHOOK(shoes_app_slot), 0);
    rb_define_method(cApp, "set_window_icon_path", CASTHOOK(shoes_app_set_icon), 1); // New in 3.2.19
    rb_define_method(cApp, "set_window_title", CASTHOOK(shoes_app_set_wtitle), 1); // New in 3.2.19
    rb_define_method(cApp, "opacity", CASTHOOK(shoes_app_get_opacity), 0);
    rb_define_method(cApp, "opacity=", CASTHOOK(shoes_app_set_opacity), 1);
    rb_define_method(cApp, "decorated", CASTHOOK(shoes_app_get_decoration), 0);
    rb_define_method(cApp, "decorated=", CASTHOOK(shoes_app_set_decoration), 1);
    rb_define_alias(cApp, "decorated?", "decorated");

    cDialog = rb_define_class_under(cTypes, "Dialog", cApp);

    eInvMode = rb_define_class_under(cTypes, "InvalidModeError", rb_eStandardError);
    eNotImpl = rb_define_class_under(cTypes, "NotImplementedError", rb_eStandardError);
    eImageError = rb_define_class_under(cTypes, "ImageError", rb_eStandardError);
    C(HEX_SOURCE, "/^(?:0x|#)?([0-9A-F]{2})([0-9A-F]{2})([0-9A-F]{2})$/i");
    C(HEX3_SOURCE, "/^(?:0x|#)?([0-9A-F])([0-9A-F])([0-9A-F])$/i");
    C(RGB_SOURCE, "/^rgb\\((\\d+), *(\\d+), *(\\d+)\\)$/i");
    C(RGBA_SOURCE, "/^rgb\\((\\d+), *(\\d+), *(\\d+), *(\\d+)\\)$/i");
    C(GRAY_SOURCE, "/^gray\\((\\d+)\\)$/i");
    C(GRAYA_SOURCE, "/^gray\\((\\d+), *(\\d+)\\)$/i");
    C(LF, "/\\r?\\n/");
    rb_eval_string(
        "def Shoes.escape(string);"
        "string.gsub(/&/n, '&amp;').gsub(/\\\"/n, '&quot;').gsub(/>/n, '&gt;').gsub(/</n, '&lt;');"
        "end"
    );

    rb_define_singleton_method(cShoes, "APPS", CASTHOOK(shoes_apps_get), 0);
    rb_define_singleton_method(cShoes, "app", CASTHOOK(shoes_app_main), -1);
    rb_define_singleton_method(cShoes, "p", CASTHOOK(shoes_p), 1);
    rb_define_singleton_method(cShoes, "log", CASTHOOK(shoes_log), 0);
    rb_define_singleton_method(cShoes, "show_console", CASTHOOK(shoes_app_console), 0); // New in 3.2.23
    rb_define_singleton_method(cShoes, "terminal", CASTHOOK(shoes_app_terminal), -1); // New in 3.3.2 replaces console
    rb_define_singleton_method(cShoes, "quit", CASTHOOK(shoes_app_quit), 0); // Shoes 3.3.2
    //rb_define_singleton_method(cShoes, "exit", CASTHOOK(shoes_app_quit), 0);

    //
    // Canvas methods
    // See ruby.h for the complete list of Canvas method signatures.
    // Macros are used to build App redirection methods, which should be
    // speedier than method_missing.
    //

    CANVAS_DEFS(RUBY_M);

    //
    // Shoes Kernel methods
    //
    rb_define_method(rb_mKernel, "pattern", CASTHOOK(shoes_pattern_method), 1);
    //rb_define_method(rb_mKernel, "quit", CASTHOOK(shoes_app_quit), 0);
    //rb_define_method(rb_mKernel, "exit", CASTHOOK(shoes_app_quit), 0);
    rb_define_method(rb_mKernel, "secret_exit_hook", CASTHOOK(shoes_exit_setup),0); //unused?

    rb_define_method(rb_mKernel, "debug", CASTHOOK(shoes_canvas_debug), 1);
    rb_define_method(rb_mKernel, "info", CASTHOOK(shoes_canvas_info), 1);
    rb_define_method(rb_mKernel, "warn", CASTHOOK(shoes_canvas_warn), 1);
    rb_define_method(rb_mKernel, "error", CASTHOOK(shoes_canvas_error), 1);

    cFlow       = rb_define_class_under(cTypes, "Flow", cShoes);
    cStack      = rb_define_class_under(cTypes, "Stack", cShoes);
    cMask       = rb_define_class_under(cTypes, "Mask", cShoes);
    cWidget     = rb_define_class_under(cTypes, "Widget", cShoes);

    cImage    = rb_define_class_under(cTypes, "Image", rb_cObject);
    rb_define_alloc_func(cImage, shoes_image_alloc);
    rb_define_method(cImage, "[]", CASTHOOK(shoes_image_get_pixel), 2);
    rb_define_method(cImage, "[]=", CASTHOOK(shoes_image_set_pixel), 3);
    rb_define_method(cImage, "nostroke", CASTHOOK(shoes_canvas_nostroke), 0);
    rb_define_method(cImage, "stroke", CASTHOOK(shoes_canvas_stroke), -1);
    rb_define_method(cImage, "strokewidth", CASTHOOK(shoes_canvas_strokewidth), 1);
    rb_define_method(cImage, "dash", CASTHOOK(shoes_canvas_dash), 1);
    rb_define_method(cImage, "cap", CASTHOOK(shoes_canvas_cap), 1);
    rb_define_method(cImage, "nofill", CASTHOOK(shoes_canvas_nofill), 0);
    rb_define_method(cImage, "fill", CASTHOOK(shoes_canvas_fill), -1);
    rb_define_method(cImage, "arrow", CASTHOOK(shoes_canvas_arrow), -1);
    rb_define_method(cImage, "line", CASTHOOK(shoes_canvas_line), -1);
    rb_define_method(cImage, "arc", CASTHOOK(shoes_canvas_arc), -1);
    rb_define_method(cImage, "oval", CASTHOOK(shoes_canvas_oval), -1);
    rb_define_method(cImage, "rect", CASTHOOK(shoes_canvas_rect), -1);
    rb_define_method(cImage, "shape", CASTHOOK(shoes_canvas_shape), -1);
    rb_define_method(cImage, "star", CASTHOOK(shoes_canvas_star), -1);
    rb_define_method(cImage, "move_to", CASTHOOK(shoes_canvas_move_to), 2);
    rb_define_method(cImage, "line_to", CASTHOOK(shoes_canvas_line_to), 2);
    rb_define_method(cImage, "curve_to", CASTHOOK(shoes_canvas_curve_to), 6);
    rb_define_method(cImage, "arc_to", CASTHOOK(shoes_canvas_arc_to), 6);
    rb_define_method(cImage, "blur", CASTHOOK(shoes_canvas_blur), -1);
    rb_define_method(cImage, "glow", CASTHOOK(shoes_canvas_glow), -1);
    rb_define_method(cImage, "shadow", CASTHOOK(shoes_canvas_shadow), -1);
    rb_define_method(cImage, "image", CASTHOOK(shoes_canvas_image), -1);
    rb_define_method(cImage, "path", CASTHOOK(shoes_image_get_path), 0);
    rb_define_method(cImage, "path=", CASTHOOK(shoes_image_set_path), 1);
    rb_define_method(cImage, "app", CASTHOOK(shoes_canvas_get_app), 0);
    rb_define_method(cImage, "displace", CASTHOOK(shoes_image_displace), 2);
    rb_define_method(cImage, "draw", CASTHOOK(shoes_image_draw), 2);
    rb_define_method(cImage, "size", CASTHOOK(shoes_image_size), 0);
    rb_define_method(cImage, "move", CASTHOOK(shoes_image_move), 2);
    rb_define_method(cImage, "parent", CASTHOOK(shoes_image_get_parent), 0);
    rb_define_method(cImage, "rotate", CASTHOOK(shoes_image_rotate), 1);
    rb_define_method(cImage, "scale", CASTHOOK(shoes_image_scale), -1);
    rb_define_method(cImage, "skew", CASTHOOK(shoes_image_skew), -1);
    rb_define_method(cImage, "transform", CASTHOOK(shoes_image_transform), 1);
    rb_define_method(cImage, "translate", CASTHOOK(shoes_image_translate), 2);
    rb_define_method(cImage, "top", CASTHOOK(shoes_image_get_top), 0);
    rb_define_method(cImage, "left", CASTHOOK(shoes_image_get_left), 0);
    rb_define_method(cImage, "width", CASTHOOK(shoes_image_get_width), 0);
    rb_define_method(cImage, "height", CASTHOOK(shoes_image_get_height), 0);
    rb_define_method(cImage, "full_width", CASTHOOK(shoes_image_get_full_width), 0);
    rb_define_method(cImage, "full_height", CASTHOOK(shoes_image_get_full_height), 0);
    rb_define_method(cImage, "remove", CASTHOOK(shoes_basic_remove), 0);
    rb_define_method(cImage, "style", CASTHOOK(shoes_image_style), -1);
    rb_define_method(cImage, "hide", CASTHOOK(shoes_image_hide), 0);
    rb_define_method(cImage, "show", CASTHOOK(shoes_image_show), 0);
    rb_define_method(cImage, "toggle", CASTHOOK(shoes_image_toggle), 0);
    rb_define_method(cImage, "click", CASTHOOK(shoes_image_click), -1);
    rb_define_method(cImage, "release", CASTHOOK(shoes_image_release), -1);
    rb_define_method(cImage, "hover", CASTHOOK(shoes_image_hover), -1);
    rb_define_method(cImage, "leave", CASTHOOK(shoes_image_leave), -1);

    cPattern = rb_define_class_under(cTypes, "Pattern", rb_cObject);
    rb_define_alloc_func(cPattern, shoes_pattern_alloc);
    rb_define_method(cPattern, "displace", CASTHOOK(shoes_pattern_displace), 2);
    rb_define_method(cPattern, "move", CASTHOOK(shoes_pattern_move), 2);
    rb_define_method(cPattern, "remove", CASTHOOK(shoes_basic_remove), 0);
    rb_define_method(cPattern, "to_pattern", CASTHOOK(shoes_pattern_self), 0);
    rb_define_method(cPattern, "style", CASTHOOK(shoes_pattern_style), -1);
    rb_define_method(cPattern, "fill", CASTHOOK(shoes_pattern_get_fill), 0);
    rb_define_method(cPattern, "fill=", CASTHOOK(shoes_pattern_set_fill), 1);
    rb_define_method(cPattern, "hide", CASTHOOK(shoes_pattern_hide), 0);
    rb_define_method(cPattern, "show", CASTHOOK(shoes_pattern_show), 0);
    rb_define_method(cPattern, "toggle", CASTHOOK(shoes_pattern_toggle), 0);
    cBackground = rb_define_class_under(cTypes, "Background", cPattern);
    rb_define_method(cBackground, "draw", CASTHOOK(shoes_background_draw), 2);
    cBorder = rb_define_class_under(cTypes, "Border", cPattern);
    rb_define_method(cBorder, "draw", CASTHOOK(shoes_border_draw), 2);

    cTextBlock = rb_define_class_under(cTypes, "TextBlock", rb_cObject);
    rb_define_alloc_func(cTextBlock, shoes_textblock_alloc);
    rb_define_method(cTextBlock, "app", CASTHOOK(shoes_canvas_get_app), 0);
    rb_define_method(cTextBlock, "contents", CASTHOOK(shoes_textblock_children), 0);
    rb_define_method(cTextBlock, "children", CASTHOOK(shoes_textblock_children), 0);
    rb_define_method(cTextBlock, "parent", CASTHOOK(shoes_textblock_get_parent), 0);
    rb_define_method(cTextBlock, "displace", CASTHOOK(shoes_textblock_displace), 2);
    rb_define_method(cTextBlock, "draw", CASTHOOK(shoes_textblock_draw), 2);
    rb_define_method(cTextBlock, "cursor=", CASTHOOK(shoes_textblock_set_cursor), 1);
    rb_define_method(cTextBlock, "cursor", CASTHOOK(shoes_textblock_get_cursor), 0);
    rb_define_method(cTextBlock, "cursor_left", CASTHOOK(shoes_textblock_cursorx), 0);
    rb_define_method(cTextBlock, "cursor_top", CASTHOOK(shoes_textblock_cursory), 0);
    rb_define_method(cTextBlock, "highlight", CASTHOOK(shoes_textblock_get_highlight), 0);
    rb_define_method(cTextBlock, "hit", CASTHOOK(shoes_textblock_hit), 2);
    rb_define_method(cTextBlock, "marker=", CASTHOOK(shoes_textblock_set_marker), 1);
    rb_define_method(cTextBlock, "marker", CASTHOOK(shoes_textblock_get_marker), 0);
    rb_define_method(cTextBlock, "move", CASTHOOK(shoes_textblock_move), 2);
    rb_define_method(cTextBlock, "top", CASTHOOK(shoes_textblock_get_top), 0);
    rb_define_method(cTextBlock, "left", CASTHOOK(shoes_textblock_get_left), 0);
    rb_define_method(cTextBlock, "width", CASTHOOK(shoes_textblock_get_width), 0);
    rb_define_method(cTextBlock, "height", CASTHOOK(shoes_textblock_get_height), 0);
    rb_define_method(cTextBlock, "remove", CASTHOOK(shoes_basic_remove), 0);
    rb_define_method(cTextBlock, "to_s", CASTHOOK(shoes_textblock_string), 0);
    rb_define_method(cTextBlock, "text", CASTHOOK(shoes_textblock_string), 0);
    rb_define_method(cTextBlock, "text=", CASTHOOK(shoes_textblock_replace), -1);
    rb_define_method(cTextBlock, "replace", CASTHOOK(shoes_textblock_replace), -1);
    rb_define_method(cTextBlock, "style", CASTHOOK(shoes_textblock_style_m), -1);
    rb_define_method(cTextBlock, "hide", CASTHOOK(shoes_textblock_hide), 0);
    rb_define_method(cTextBlock, "show", CASTHOOK(shoes_textblock_show), 0);
    rb_define_method(cTextBlock, "toggle", CASTHOOK(shoes_textblock_toggle), 0);
    rb_define_method(cTextBlock, "click", CASTHOOK(shoes_textblock_click), -1);
    rb_define_method(cTextBlock, "release", CASTHOOK(shoes_textblock_release), -1);
    rb_define_method(cTextBlock, "hover", CASTHOOK(shoes_textblock_hover), -1);
    rb_define_method(cTextBlock, "leave", CASTHOOK(shoes_textblock_leave), -1);
    cPara = rb_define_class_under(cTypes, "Para", cTextBlock);
    cBanner = rb_define_class_under(cTypes, "Banner", cTextBlock);
    cTitle = rb_define_class_under(cTypes, "Title", cTextBlock);
    cSubtitle = rb_define_class_under(cTypes, "Subtitle", cTextBlock);
    cTagline = rb_define_class_under(cTypes, "Tagline", cTextBlock);
    cCaption = rb_define_class_under(cTypes, "Caption", cTextBlock);
    cInscription = rb_define_class_under(cTypes, "Inscription", cTextBlock);

    rb_define_method(cApp, "method_missing", CASTHOOK(shoes_app_method_missing), -1);

    rb_define_method(rb_mKernel, "alert", CASTHOOK(shoes_dialog_alert), -1);
    rb_define_method(rb_mKernel, "ask", CASTHOOK(shoes_dialog_ask), -1);
    rb_define_method(rb_mKernel, "confirm", CASTHOOK(shoes_dialog_confirm), -1);
    rb_define_method(rb_mKernel, "ask_color", CASTHOOK(shoes_dialog_color), 1);
    rb_define_method(rb_mKernel, "ask_open_file", CASTHOOK(shoes_dialog_open), -1);
    rb_define_method(rb_mKernel, "ask_save_file", CASTHOOK(shoes_dialog_save), -1);
    rb_define_method(rb_mKernel, "ask_open_folder", CASTHOOK(shoes_dialog_open_folder), -1);
    rb_define_method(rb_mKernel, "ask_save_folder", CASTHOOK(shoes_dialog_save_folder), -1);
    rb_define_method(rb_mKernel, "font", CASTHOOK(shoes_font), 1);
    
    SHOES_TYPES_INIT;
}

VALUE shoes_exit_setup(VALUE self) {
    rb_define_method(rb_mKernel, "quit", CASTHOOK(shoes_app_quit), 0);
    rb_define_method(rb_mKernel, "exit", CASTHOOK(shoes_app_quit), 0);
    return Qtrue;
}
