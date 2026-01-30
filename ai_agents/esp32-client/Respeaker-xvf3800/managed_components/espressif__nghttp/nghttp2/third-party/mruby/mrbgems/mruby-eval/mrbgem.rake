MRuby::Gem::Specification.new('mruby-eval') do |spec|
  spec.license = 'MIT'
  spec.author  = 'mruby developers'
  spec.summary = 'standard Kernel#eval method'

  add_dependency 'mruby-compiler', :core => 'mruby-compiler'
  add_dependency 'mruby-binding', :core => 'mruby-binding'
  spec.add_test_dependency('mruby-metaprog', :core => 'mruby-metaprog')
  spec.add_test_dependency('mruby-method', :core => 'mruby-method')
end
