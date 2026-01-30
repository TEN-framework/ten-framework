set(CONFIGFILES
  nghttpx-init
  nghttpx.service
  nghttpx-upstart.conf
)

# Note that the execute permissions of nghttpx-init is preserved
foreach(name IN LISTS CONFIGFILES)
  configure_file("${name}.in" "${name}" @ONLY)
endforeach()

# set(EXTRA_DIST ${CONFIGFILES} nghttpx-logrotate tlsticketupdate.go)
