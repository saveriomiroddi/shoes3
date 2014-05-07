#
# Build shoes for Raspberry pi.  Ruby is either cross compiled
# or copied from an .rvm build on the pi.  The system headers and
# libs come from a schroot (debrpi) and/or the cross compiler.
# Curl *may* also separately compiled and located. Or in the chroot.
#
#ENV['DEBUG'] = "true" # turns on the tracing log
#ENV['GTK'] = "gtk+-3.0" # pick this or "gtk+-2.0"
ENV['GTK'] = "gtk+-2.0"
# I don't recommend try to copy Gtk2 -it only works mysteriously
COPY_GTK = false 
ENV['GDB'] = nil # compile -g,  strip symbols when nil
CHROOT = "/srv/chroot/debrpi"
# Where does ruby code live? For the pi, I build one  
# inside the chroot but we access it from outside.
EXT_RUBY = "/srv/chroot/debrpi/usr/local"
SHOES_TGT_ARCH = "armv7l-linux-eabihf"
# Specify where the Target system binaries live. 
# Trailing slash is important.
TGT_SYS_DIR = "#{CHROOT}/"
# Setup some shortcuts for the library locations. 
# These are not ruby paths but library paths
arch = 'arm-linux-gnueabihf'
uldir = "#{TGT_SYS_DIR}usr/lib"
ularch = "#{TGT_SYS_DIR}usr/lib/#{arch}"
larch = "#{TGT_SYS_DIR}lib/#{arch}"
# Set appropriately (in my PATH, or use abs)
CC = "arm-linux-gnueabihf-gcc"
# These ENV vars are used by the extconf.rb files (and tasks.rb)
ENV['SYSROOT'] = CHROOT
ENV['CC'] = CC
ENV['TGT_RUBY_PATH'] = EXT_RUBY
ENV['TGT_ARCH'] = SHOES_TGT_ARCH
#ENV['TGT_RUBY_V'] = '1.9.1'
ENV['TGT_RUBY_V'] = '2.0.0'
TGT_RUBY_V = ENV['TGT_RUBY_V'] 
#pkgruby ="#{EXT_RUBY}/lib/pkgconfig/ruby-1.9.pc"
pkgruby ="#{EXT_RUBY}/lib/pkgconfig/ruby-2.0.pc"
pkggtk ="#{ularch}/pkgconfig/#{ENV['GTK']}.pc" 
# CURL or RUBY?
RUBY_HTTP = true
if !RUBY_HTTP
# where is curl (lib,include)
curlloc = "#{TGT_SYS_DIR}usr/lib/arm-linux-gnueabihf/pkgconfig/libcurl.pc"
CURL_LDFLAGS = `pkg-config --libs #{curlloc}`.strip
CURL_CFLAGS = `pkg-config --cflags #{curlloc}`.strip
end

ENV['PKG_CONFIG_PATH'] = "#{ularch}/pkgconfig"

if RUBY_HTTP
  file_list = %w{shoes/native/gtk.c shoes/http/rbload.c} + ["shoes/*.c"]
else
  file_list = %w{shoes/native/gtk.c shoes/http/curl.c} + ["shoes/*.c"]
end
SRC = FileList[*file_list]
OBJ = SRC.map do |x|
  x.gsub(/\.\w+$/, '.o')
end

ADD_DLL = []

# Hand code for your situation and Ruby purity. Mine is
# "Church of Whatever Works That I Can Understand"
def xfixip(path)
   path.gsub!(/-I\/usr\//, "-I#{TGT_SYS_DIR}usr/")
   return path
end

def xfixrvmp(path)
  #puts "path  in: #{path}"
  path.gsub!(/-I\/home\/ccoupe\/\.rvm/, "-I/home/cross/armv6-pi/rvm")
  path.gsub!(/-I\/usr\/local\//, "-I#{TGT_SYS_DIR}usr/local/")
  #puts "path out: #{path}"
  return path
end

#  fix up the -L paths for rvm ruby. Undo when not using an rvm ruby
def xfixrvml(path)
  #path.gsub!(/-L\/home\/ccoupe\/\.rvm/, "-L#{TGT_SYS_DIR}rvm")
  return path
end

# fixup the -L paths for gtk and other libs
def xfixil(path) 
  path.gsub!(/-L\/usr\/lib/, "-L#{TGT_SYS_DIR}usr/lib")
  return path
end

# Target environment
CAIRO_CFLAGS = `pkg-config --cflags "#{ularch}/pkgconfig/cairo.pc"`.strip
CAIRO_LIB = `pkg-config --libs "#{ularch}/pkgconfig/cairo.pc"`.strip
PANGO_CFLAGS = `pkg-config --cflags "#{ularch}/pkgconfig/pango.pc"`.strip
PANGO_LIB = `pkg-config --libs "#{ularch}/pkgconfig/pango.pc"`.strip

png_lib = 'png'

if ENV['DEBUG'] || ENV['GDB']
  LINUX_CFLAGS = " -g -O0"
else
#  LINUX_CFLAGS = " -O -Wall"
  LINUX_CFLAGS = " -O"
end
LINUX_CFLAGS << " -DRUBY_HTTP" if RUBY_HTTP
LINUX_CFLAGS << " -DSHOES_GTK " 
LINUX_CFLAGS << " -DGTK3 " unless ENV['GTK'] == 'gtk+-2.0'
LINUX_CFLAGS << xfixrvmp(`pkg-config --cflags "#{pkgruby}"`.strip)+" "
LINUX_CFLAGS << " -I#{TGT_SYS_DIR}usr/include/#{arch} #{CURL_CFLAGS  if !RUBY_HTTP} "

LINUX_CFLAGS << xfixip("-I/usr/include")+" "
LINUX_CFLAGS << xfixip(`pkg-config --cflags "#{pkggtk}"`.strip)+" "

#LINUX_CFLAGS << " #{CAIRO_CFLAGS} #{PANGO_CFLAGS} "

LINUX_LIB_NAMES = %W[ungif jpeg]

DLEXT = "so"
LINUX_LDFLAGS = "-fPIC -shared --sysroot=#{CHROOT} -L#{ularch} "
LINUX_LDFLAGS << `pkg-config --libs "#{pkggtk}"`.strip+" "
# dont use the ruby link info
RUBY_LDFLAGS = "-rdynamic -Wl,-export-dynamic "
RUBY_LDFLAGS << "-L#{EXT_RUBY}/lib -lruby "
#RUBY_LDFLAGS << "-L#{ularch} -lrt -ldl -lcrypt -lm "
#LINUX_LDFLAGS << " #{CURL_LDFLAGS}"
#LINUX_LDFLAGS << " -L. -rdynamic -Wl,-export-dynamic"

LINUX_LIBS = "--sysroot=#{CHROOT} -L/usr/lib "
LINUX_LIBS << LINUX_LIB_NAMES.map { |x| "-l#{x}" }.join(' ')

LINUX_LIBS << " #{RUBY_LDFLAGS} #{CAIRO_LIB} #{PANGO_LIB} #{CURL_LDFLAGS if !RUBY_HTTP} "

# This could be precomputed by rake linux:setup:xxx 
# but for now make a hash of all the dep libs that need to be copied.
# and their location. This should be used in pre_build instead of 
# copy_deps_to_dist, although either would work. 
SOLOCS = {}
SOLOCS['curl'] = "#{ularch}/libcurl.so.4" if !RUBY_HTTP
SOLOCS['ungif'] = "#{uldir}/libungif.so.4"
SOLOCS['gif'] = "#{uldir}/libgif.so.4"
SOLOCS['jpeg'] = "#{ularch}/libjpeg.so.8"
SOLOCS['libyaml'] = "#{ularch}/libyaml-0.so.2"
SOLOCS['crypto'] = "#{ularch}/libcrypto.so.1.0.0"
SOLOCS['ssl'] = "#{ularch}/libssl.so.1.0.0"
if ENV['GTK'] == 'gtk+-2.0' && COPY_GTK == true
  SOLOCS['gtk2'] = "#{ularch}/libgtk-x11-2.0.so.0"
  SOLOCS['gdk2'] = "#{ularch}/libgdk-x11-2.0.so.0"
  SOLOCS['atk'] = "#{ularch}/libatk-1.0.so.0"
  SOLOCS['gio'] = "#{ularch}/libgio-2.0.so.0"
  SOLOCS['pangoft2'] = "#{ularch}/libpangoft2-1.0.so.0"
  SOLOCS['pangocairo'] = "#{ularch}/libpangocairo-1.0.so.0"
  SOLOCS['gdk_pixbuf'] = "#{ularch}/libgdk_pixbuf-2.0.so.0"
  SOLOCS['cairo'] = "#{ularch}/libcairo.so.2"
  SOLOCS['pango'] = "#{ularch}/libpango-1.0.so.0"
  SOLOCS['freetype'] = "#{ularch}/libfreetype.so.6"
  SOLOCS['fontconfig'] = "#{ularch}/libfontconfig.so.1"
  SOLOCS['pixman'] = "#{ularch}/libpixman-1.so.0"
  SOLOCS['gobject'] = "#{ularch}/libgobject-2.0.so.0"
  SOLOCS['glib'] = "#{larch}/libglib-2.0.so.0"
end