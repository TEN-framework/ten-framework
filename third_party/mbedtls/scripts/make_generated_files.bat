@rem Generate automatically-generated configuration-independent source files
@rem and build scripts.
@rem Perl and Python 3 must be on the PATH.
@rem psa_crypto_driver_wrappers.c needs to be generated prior to
@rem generate_visualc_files.pl being invoked.
python scripts\generate_driver_wrappers.py || exit /b 1
perl scripts\generate_errors.pl || exit /b 1
perl scripts\generate_query_config.pl || exit /b 1
perl scripts\generate_features.pl || exit /b 1
python scripts\generate_ssl_debug_helpers.py || exit /b 1
perl scripts\generate_visualc_files.pl || exit /b 1
python scripts\generate_psa_constants.py || exit /b 1
python tests\scripts\generate_psa_tests.py || exit /b 1
