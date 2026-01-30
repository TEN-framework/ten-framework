# mruby-set

Set class

## Example

```ruby
set1 = Set.new([1,2])
set2 = Set[1,2,3]
set3 = Set[4]

set1 + set3
#=> #<Set: {1, 2, 4}>
set2 - set1
#=> #<Set: {3}>
set2 & set1
#=> #<Set: {1, 2}>
set1 ^ set2
#=> #<Set: {3}>
```

## Limitations

These methods are not implemented yet:

- freeze
- to_set
- divide(Set#divide with 2 arity block is not implemented.)

## License

Under the MIT License:

- see [LICENSE](LICENSE) file
