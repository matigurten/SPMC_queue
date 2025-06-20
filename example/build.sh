g++ -O3 -o multhread multhread.cc -pthread
g++ -O3 -o shm_writer shm_writer.cc -lrt
g++ -O3 -o shm_reader shm_reader.cc -lrt
g++ -O3 -c order_book_manager.cc -o order_book_manager.o

g++ -O3 -o test_order_book_manager test_order_book_manager.cc order_book_manager.o
