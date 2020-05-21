project('nrfdfu', 'c')

cc = meson.get_compiler('c')

libsystemd = dependency('libsystemd', required: get_option('ble_support'))
blzlib = dependency('blzlib', required: get_option('ble_support'))
libzip = dependency('libzip')
jsonc = dependency('json-c')
zlib = dependency('zlib')

if get_option('ble_support').enabled()
	add_global_arguments('-DBLE_SUPPORT', language : 'c')
endif

executable('nrfdfu',
	'main.c', 'log.c', 'util.c', 'serialtty.c',
    'dfu.c', 'dfu_serial.c', 'slip.c', 'dfu_ble.c',
	dependencies : [ libsystemd, blzlib, libzip, jsonc, zlib ],
	install: true, install_dir : 'sbin')
