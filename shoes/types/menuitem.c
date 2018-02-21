#include "shoes/types/native.h"
#include "shoes/types/menuitem.h"
/*
 * A top Level menu in menubar  -File, Edit, Help...
 */ 
// ruby
VALUE cShoesMenuitem;

FUNC_M("+menuitem", menuitem, -1);

void shoes_menuitem_init() {
    cShoesMenuitem  = rb_define_class_under(cTypes, "Menuitem", rb_cObject);
    rb_define_method(cShoesMenuitem, "title", CASTHOOK(shoes_menuitem_gettitle), 0);
    rb_define_method(cShoesMenuitem, "title=", CASTHOOK(shoes_menuitem_settitle), 1);
    rb_define_method(cShoesMenuitem, "key", CASTHOOK(shoes_menuitem_getkey), 0);
    rb_define_method(cShoesMenuitem, "key=", CASTHOOK(shoes_menuitem_setkey), 1);
    rb_define_method(cShoesMenuitem, "block=", CASTHOOK(shoes_menuitem_setblk), 2);
    RUBY_M("+menuitem", menuitem, -1);
}

void shoes_menuitem_mark(shoes_menuitem *mi) {
    rb_gc_mark_maybe(mi->block);
    rb_gc_mark_maybe(mi->context);
}

static void shoes_menuitem_free(shoes_menuitem *mi) {
    RUBY_CRITICAL(SHOE_FREE(mi));
}

VALUE shoes_menuitem_alloc(VALUE klass) {
    VALUE obj;
    shoes_menuitem *mi = SHOE_ALLOC(shoes_menuitem);
    SHOE_MEMZERO(mi, shoes_menuitem, 1);
    obj = Data_Wrap_Struct(klass, shoes_menuitem_mark, shoes_menuitem_free, mi);
    mi->native = NULL;
    mi->title = NULL;
    mi->key = NULL;
    mi->block = Qnil;
    mi->context = Qnil;
    return obj;
}

VALUE shoes_menuitem_new(VALUE text, VALUE key, VALUE blk, VALUE canvas) {
  VALUE obj= shoes_menuitem_alloc(cShoesMenuitem);
  shoes_menuitem *mi;
  int sep = 0;
  Data_Get_Struct(obj, shoes_menuitem, mi);
  mi->title = RSTRING_PTR(text);
  //fprintf(stderr,"mi: %s\n",mi->title);
  if (strncmp(mi->title, "---", 3) == 0) {
    mi->title = NULL;
    mi->key = NULL;
    mi->block = Qnil;
    mi->native = shoes_native_menusep_new(mi);
    mi->context = Qnil;  // never called
  } else {
    if (TYPE(key) == T_STRING) 
      mi->key = RSTRING_PTR(key);
    else
      mi->key = NULL;
    mi->block = blk;
    mi->native = shoes_native_menuitem_new(mi);
    shoes_canvas *cvs;
    Data_Get_Struct(canvas, shoes_canvas, cvs);
    shoes_app *app = cvs->app;
    // TODO: mi->context = app->canvas; or canvas? 
    mi->context = canvas;
  }
  return obj;
}


VALUE shoes_menuitem_gettitle(VALUE self) {
  shoes_menuitem *mi;
  Data_Get_Struct(self, shoes_menuitem, mi);
  if (mi->title == 0)
    return rb_str_new2("");
  else
   return rb_str_new2(mi->title);
}

VALUE shoes_menuitem_settitle(VALUE self, VALUE text) {
  return Qnil;
}

VALUE shoes_menuitem_getkey(VALUE self) {
  return Qnil;
}

VALUE shoes_menuitem_setkey(VALUE self, VALUE text) {
  return Qnil;
}

VALUE shoes_menuitem_setblk(VALUE self, VALUE block) {
  return Qnil;
}


// canvas - Shoes usage:  menuitem "Quit" , key: 'control_q'  do Shoes.quit end
//    just like a button. 
VALUE shoes_canvas_menuitem(int argc, VALUE *argv, VALUE self) {
    rb_arg_list args;
    VALUE text = Qnil, attr = Qnil, blk = Qnil, keystr = Qnil;
    VALUE menuitem = Qnil;

    switch (rb_parse_args(argc, argv, "s|h,|h", &args)) {
        case 1:
            text = args.a[0];
            attr = args.a[1];
            break;

        case 2:
            attr = args.a[0];
            break;
    }
    
    keystr = shoes_hash_get(attr, rb_intern("key"));
    
    if (rb_block_given_p()) {
        //ATTRSET(attr, click, rb_block_proc());
        blk = rb_block_proc();
    }
    menuitem = shoes_menuitem_new(text, keystr, blk, self);
    
    return menuitem;
}

