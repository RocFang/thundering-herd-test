all:
	gcc accept_test.c -o accept_test
	gcc epoll_test.c -o epoll_test
clean:
	rm -rf accept_test epoll_test
