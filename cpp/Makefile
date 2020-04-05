test-json: bin/json_test
	bin/json_test

bin/json_test: json.hpp json_test.cc
	@mkdir -p $(dir $@)
	g++ -o $@ $(CCFLAGS) json_test.cc -lgtest -lpthread
