MRuby::Gem::Specification.new('mruby-socket') do |spec|
  spec.license = 'MIT'
  spec.authors = ['Internet Initiative Japan Inc.', 'mruby developers']
  spec.summary = 'standard socket class'

  #spec.cc.defines << "HAVE_SA_LEN=0"

  # If Windows, use winsock
  if spec.for_windows?
    spec.linker.libraries << "wsock32"
    spec.linker.libraries << "ws2_32"
  end

  spec.add_dependency('mruby-io', :core => 'mruby-io')
  spec.add_dependency('mruby-error', :core => 'mruby-error')
  # spec.add_dependency('mruby-mtest')
end
