module Make
  include FileUtils
  
  def cc(t)
    sh "#{CC} -I. -c -o#{t.name} #{WINDOWS_CFLAGS} #{t.source}"
  end

  # Subs in special variables
  def rewrite before, after, reg = /\#\{(\w+)\}/, reg2 = '\1'
    File.open(after, 'w') do |a|
      File.open(before) do |b|
        b.each do |line|
          a << line.gsub(reg) do
            if reg2.include? '\1'
              reg2.gsub(%r!\\1!, Object.const_get($1))
            else
              reg2
            end
          end
        end
      end
    end
  end

end

include FileUtils

class MakeMinGW
  extend Make

  class << self

    def copy_deps_to_dist
      puts "copy_deps_to_dist dir=#{pwd}"
      unless APP['GDB']
        sh "#{STRIP}  #{TGT_DIR}/*.dll"
        Dir.glob("#{TGT_DIR}/lib/ruby/**/*.so").each {|lib| sh "#{STRIP} #{lib}"}
      end
    end

    def setup_system_resources
      cp APP['icons']['gtk'], "#{TGT_DIR}/static/app-icon.png"
    end
   
    # this is called from the file task based new_builder 
    def new_so (name) 
      $stderr.puts "new so: #{name}"
      #tgts = name.split('/')
      #tgtd = tgts[0]
      tgtd = File.dirname(name)
      objs = []
      SubDirs.each do |f|
        d = File.dirname(f)
        objs = objs + FileList["#{d}/*.o"]      
      end
      # TODO  fix: gtk - needs to dig deeper vs osx
      objs = objs + FileList["shoes/native/gtk/*.o"]
      main_o = 'shoes/main.o'
      objs = objs - [main_o]
      sh "#{CC} -o #{tgtd}/libshoes.#{DLEXT} #{objs.join(' ')} #{LINUX_LDFLAGS} #{LINUX_LIBS}"
    end

    def new_link(name)
      $stderr.puts "new_link: #{name}"
      dpath = File.dirname(name)
      fname = File.basename(name)
      tgts = name.split('/')
      tgtd = tgts[0]
      bin = "#{dpath}/shoes.exe"
      binc = "#{dpath}/cshoes.exe"
      rm_f bin
      rm_f binc
      tp = "#{TGT_DIR}/#{APP['Bld_Tmp']}"
      sh "#{WINDRES} -I. shoes/appwin32.rc shoes/appwin32.o"
      missing = "-lgtk-3 -lgdk-3 -lfontconfig-1 -lpangocairo-1.0" # TODO: This is a bug in env.rb for 
      sh "#{CC} -o #{bin} #{tp}/main.o shoes/appwin32.o -L#{TGT_DIR} -lshoes -mwindows  #{LINUX_LIBS} #{missing}"
      sh "#{STRIP} #{bin}" unless APP['GDB']
      sh "#{CC} -o #{binc} #{tp}/main.o shoes/appwin32.o -L#{TGT_DIR} -lshoes #{LINUX_LIBS}  #{missing}"
      sh "#{STRIP} #{binc}" unless APP['GDB']
    end   
   
    # does nothing
    def make_userinstall
    end
 
    
    def make_installer
      # assumes you have NSIS installed on your box in the system PATH 
      def sh(*args); super; end
      $stderr.puts "make_installer #{`pwd`} moving tmp/"
      tp = "#{TGT_DIR}/#{APP['Bld_Tmp']}"
      mp = "#{TGT_DIR}-#{APP['Bld_Tmp']}"
      mv tp, mp
      mkdir_p "pkg"
      cp_r "VERSION.txt", "#{TGT_DIR}/VERSION.txt"
      rm_rf "#{TGT_DIR}/nsis"
      cp_r  "platform/msw", "#{TGT_DIR}/nsis"
      cp APP['icons']['win32'], "#{TGT_DIR}/nsis/setup.ico"
      rewrite "#{TGT_DIR}/nsis/base.nsi", "#{TGT_DIR}/nsis/#{WINFNAME}.nsi"
      Dir.chdir("#{TGT_DIR}/nsis") do
        sh "\"c:\\Program Files (x86)\\NSIS\\Unicode\\makensis.exe\" #{WINFNAME}.nsi"  
      end
      mv "#{TGT_DIR}/nsis/#{WINFNAME}.exe", "pkg/"
      $stderr.puts "restore tmp/"
      mv mp, tp
    end
  end
end
