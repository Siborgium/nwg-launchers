sources = files(
	'grid.cc',
	'grid_classes.cc',
	'grid_tools.cc'
)

executable(
	'nwggrid-daemon',
	sources,
	dependencies: [json, gtkmm],
	link_with: nwg,
	include_directories: [nwg_inc, nwg_conf_inc, json_header_dir],
	install: true
)

executable(
	'nwggrid',
	'grid_launcher.cc',
	dependencies: [gtkmm], # TODO: remove gtkmm dependency
	link_with: nwg,
	include_directories: [nwg_inc],
	install: true
)

install_data(
	['style.css', 'icon-missing.svg'],
	install_dir: conf_data.get('datadir', '') / 'nwggrid'
)
