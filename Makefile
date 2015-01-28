all:
	gcc accept_multi_process_test.c -o accept_multi_process_test.out
	gcc epoll_single_process_test.c -o epoll_single_process_test.out
	gcc epoll_multi_process_test.c -o epoll_multi_process_test.out
clean:
	rm -rf *.out
