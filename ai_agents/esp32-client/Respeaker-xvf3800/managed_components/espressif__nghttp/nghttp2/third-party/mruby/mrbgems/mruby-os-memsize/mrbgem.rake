MRuby::Gem::Specification.new('mruby-os-memsize') do |spec|
  spec.license = 'MIT'
  spec.author  = 'mruby developers'
  spec.summary = 'ObjectSpace memsize_of method'

  spec.add_dependency('mruby-objectspace', :core => 'mruby-objectspace')
  spec.add_test_dependency('mruby-metaprog', :core => 'mruby-metaprog')
  spec.add_test_dependency('mruby-method', :core => 'mruby-method')
  spec.add_test_dependency('mruby-fiber', :core => 'mruby-fiber')
end
