## Need for CI

Generally if we're adding something that's supposed to work ongoing, the stuff
should be exercised in CI (at least Travis).

If there are few users for a particular feature, experience has shown that
refactors or other upheaval can easily break it into a state of uselessness
without anyone noticing until later.

Therefore here's a description of how to add something to the CI tests... this
is certainly a nonproductive PITA and I have never been thanked for the work
involved.  But if the promise of the various features working is going to
remain alive, it's necessary to include CI test where possible with new
nontrivial code.

## Integration points

### cmake

`.travis.yml` maps the various test activities to CMake options needed.

### including dependent packages into travis

See `./scripts/travis_install.sh`

### performing prepared test actions

See `./scripts/travis_control.sh`

