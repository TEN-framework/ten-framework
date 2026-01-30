MRuby.autoload :Documentation, 'mruby/doc'

desc 'generate document'
task :doc => %w[doc:api doc:capi]

namespace :doc do
  desc 'generate yard docs'
  task :api do
    begin
      sh "mrbdoc"
    rescue
      puts "ERROR: To generate YARD documentation, you should install yard-mruby gem."
      puts "  $ gem install yard-mruby yard-coderay"
      puts "https://yardoc.org/"
      puts "https://rubygems.org/gems/yard-mruby"
      puts "https://rubygems.org/gems/yard-coderay"
    end
  end

  desc 'generate doxygen docs'
  task :capi do
    begin
      sh "doxygen Doxyfile"
    rescue
      puts "ERROR: To generate C API documents, you need Doxygen and Graphviz."
      puts "On Debian-based systems:"
      puts "  $ sudo apt-get install doxygen graphviz"
      puts "On RHEL-based systems:"
      puts "  $ sudo dnf install doxygen graphviz"
      puts "On macOS-based systems:"
      puts "  $ brew install doxygen graphviz"
      puts "https://www.doxygen.nl/"
      puts "https://graphviz.org/"
    end
  end

  desc 'clean all built docs'
  task :clean => %w[clean:api clean:capi]

  namespace :clean do
    desc 'clean yard docs'
    task :api do
      rm_rf 'doc/api'
    end

    desc 'clean doxygen docs'
    task :capi do
      rm_rf 'doc/capi'
    end
  end

  namespace :view do
    desc 'open yard docs'
    task :api do
      if RUBY_PLATFORM.include?('darwin')
        sh 'open doc/api/index.html'
      else
        sh 'xdg-open doc/api/index.html'
      end
    end

    desc 'open doxygen docs'
    task :capi do
      if RUBY_PLATFORM.include?('darwin')
        sh 'open doc/capi/html/index.html'
      else
        sh 'xdg-open doc/capi/html/index.html'
      end
    end
  end

  desc 'update doc/internal/opcode.md'
  task 'update-opcode.md' do
    unless system(*%W(git --git-dir #{MRUBY_ROOT}/.git --work-tree #{MRUBY_ROOT} diff --quiet @ -- doc/internal/opcode.md))
      abort <<~'ERRMESG'
        The file "doc/internal/opcode.md" has been modified but not committed.
        To avoid loss of your edits, the automatic update process has been aborted.
      ERRMESG
    end

    MRuby::Documentation.update_opcode_md
  end
end

# deprecated
task "api_doc" => "doc:api"
task "capi_doc" => "doc:capi"
task "clean_doc" => "doc:clean"
task "clean_api_doc" => "doc:clean:api"
task "clean_capi_doc" => "doc:clean:capi"
task "view_api" => "doc:view:api"
task "view_capi" => "doc:view:capi"
