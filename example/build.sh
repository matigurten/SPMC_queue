g++ -O3 -o multhread multhread.cc -pthread
g++ -O3 -o shm_writer shm_writer.cc -lrt
g++ -O3 -o shm_reader shm_reader.cc order_book.cc latency_stats.cc -lrt
g++ -O3 -c order_book_manager.cc -o order_book_manager.o

g++ -O3 -o test_order_book_manager test_order_book_manager.cc order_book.cc

# New snapshot consumer programs
g++ -O3 -o snapshot_consumer snapshot_consumer.cc -lrt
g++ -O3 -o snapshot_monitor snapshot_monitor.cc -lrt
g++ -O3 -o simple_consumer simple_consumer.cc -lrt
