default: tests.log

test:
	@../test.pl -p -c ../../../src/jbmc/jbmc
	@../test.pl -p -c "../../../src/jbmc/jbmc --symex-driven-lazy-loading" -X symex-driven-lazy-loading-expected-failure

tests.log: ../test.pl
	@../test.pl -p -c ../../../src/jbmc/jbmc
	@../test.pl -p -c "../../../src/jbmc/jbmc --symex-driven-lazy-loading" -X symex-driven-lazy-loading-expected-failure

show:
	@for dir in *; do \
		if [ -d "$$dir" ]; then \
			vim -o "$$dir/*.java" "$$dir/*.out"; \
		fi; \
	done;

clean:
	find -name '*.out' -execdir $(RM) '{}' \;
	find -name '*.gb' -execdir $(RM) '{}' \;
	$(RM) tests.log

%.class: %.java ../../src/org.cprover.jar
	javac -g -cp ../../src/org.cprover.jar:. $<

nondet_java_files := $(shell find . -name "Nondet*.java")
nondet_class_files := $(patsubst %.java, %.class, $(nondet_java_files))

.PHONY: nondet-class-files
nondet-class-files: $(nondet_class_files)

.PHONY: clean-nondet-class-files
clean-nondet-class-files:
	-rm $(nondet_class_files)
