g++ -O3 -o multhread multhread.cc -pthread
g++ -O3 -o shm_writer shm_writer.cc -lrt
g++ -O3 -o shm_reader shm_reader.cc order_book.cc latency_stats.cc -lrt
g++ -O3 -c order_book_manager.cc -o order_book_manager.o

g++ -O3 -o test_order_book_manager test_order_book_manager.cc order_book.cc

# New snapshot consumer programs with logger
g++ -O3 -o snapshot_consumer snapshot_consumer.cc logger.cc -lrt -pthread
g++ -O3 -o snapshot_monitor snapshot_monitor.cc -lrt
g++ -O3 -o simple_consumer simple_consumer.cc -lrt

# FOD Parquet writer for research and ML
g++ -O3 -o fod_parquet_writer fod_parquet_writer.cc -lrt -pthread

# Test FOD reader for debugging
g++ -O3 -o test_fod_reader test_fod_reader.cc -lrt -pthread
