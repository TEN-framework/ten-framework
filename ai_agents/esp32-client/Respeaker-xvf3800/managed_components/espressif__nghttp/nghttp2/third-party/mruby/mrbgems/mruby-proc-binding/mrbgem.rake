MRuby::Gem::Specification.new('mruby-proc-binding') do |spec|
  spec.license = 'MIT'
  spec.author  = 'mruby developers'
  spec.summary = 'Proc#binding method'

  spec.add_dependency('mruby-binding', :core => 'mruby-binding')
  spec.add_dependency('mruby-proc-ext', :core => 'mruby-proc-ext')
  spec.add_test_dependency('mruby-eval', :core => 'mruby-eval')
  spec.add_test_dependency('mruby-compiler', :core => 'mruby-compiler')
end
