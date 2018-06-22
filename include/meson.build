confdata = configuration_data()
confdata.set('version', meson.project_version())
install_headers(configure_file(input: 'zck.h.in',
                               output: 'zck.h',
                               configuration: confdata))
