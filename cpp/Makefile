test-json: bin/json_test
	bin/json_test

bin/json_test: json.hpp json_test.cc
	@mkdir -p $(dir $@)
	g++ -o $@ $(CCFLAGS) json_test.cc -lgtest -lpthread

bin/parse_json: parse_json.cc json.hpp
	@mkdir -p $(dir $@)
	g++ -o $@ $(CCFLAGS) parse_json.cc -lpthread

parse_coco_train2017: data/COCO/annotations/instances_train2017.json bin/parse_json
	bin/parse_json $<

parse_coco_val2017: data/COCO/annotations/instances_val2017.json bin/parse_json
	bin/parse_json $<

data/COCO/annotations/instances_train2017.json: data/COCO/annotations_trainval2017.zip
	unzip -q $< annotations/instances_train2017.json -d data/COCO
	@touch $@

data/COCO/annotations/instances_val2017.json: data/COCO/annotations_trainval2017.zip
	unzip -q $< annotations/instances_val2017.json -d data/COCO
	@touch $@

data/COCO/annotations_%.zip:
	@mkdir -p $(dir $@)
	wget -c -O $@ http://images.cocodataset.org/annotations/annotations_$*.zip
	@touch $@
