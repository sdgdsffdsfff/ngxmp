
build:
	$(MAKE) -f objs/Makefile

install:
	$(MAKE) -f objs/Makefile install

clean:
	rm -rf Makefile objs

upgrade:
	/data1/ngmp.nginx/sbin/ngmp.nginx -t

	kill -USR2 `cat /data1/ngmp.nginx/logs/nginx.pid`
	sleep 1
	test -f /data1/ngmp.nginx/logs/nginx.pid.oldbin

	kill -QUIT `cat /data1/ngmp.nginx/logs/nginx.pid.oldbin`

upgrade1:
	# upgrade 0.1.x to 0.2+

	/data1/ngmp.nginx/sbin/ngmp.nginx -t

	cp /data1/ngmp.nginx/logs/nginx.pid /data1/ngmp.nginx/logs/nginx.pid.oldbin

	kill -USR2 `cat /data1/ngmp.nginx/logs/nginx.pid`
	sleep 1
	test -f /data1/ngmp.nginx/logs/nginx.pid.oldbin

	cp /data1/ngmp.nginx/logs/nginx.pid /data1/ngmp.nginx/logs/nginx.pid.newbin

	kill -QUIT `cat /data1/ngmp.nginx/logs/nginx.pid.oldbin`
