src = [
	'src/ip/arp.cpp',
	'src/ip/checksum.cpp',
	'src/ip/icmp.cpp',
	'src/ip/ip4.cpp',
	'src/ip/tcp4.cpp',
	'src/ip/udp4.cpp',
	'src/main.cpp',
	'src/nic.cpp',
	'src/raw.cpp',
	'src/netlink/netlink.cpp',
	'src/netlink/packets.cpp',
	'src/netlink/queries.cpp',
]

inc = [ 'include', 'src' ]

dep = [ fs_proto_dep, mbus_proto_dep, svrctl_proto_dep, nic_virtio_dep,
	nic_usb_net_dep, nic_rtl8168_dep, core_dep ]

if host_machine.cpu_family() == 'x86_64'
	dep += nic_freebsd_e1000_dep
endif

executable('netserver', src,
	dependencies : dep,
	include_directories : inc,
	install : true
)

custom_target('netserver-server',
	command : [bakesvr, '-o', '@OUTPUT@', '@INPUT@'],
	output : 'netserver.bin',
	input : 'netserver.yml',
	install : true,
	install_dir : server
)
