EXTRA_DIST = test_jul JULTest.java

if BUILD_JNI_INTERFACE

if HAVE_JAVA_JDK
JCC=$(JAVA_JDK)/bin/javac
else
JCC=javac
endif

AM_CPPFLAGS = -I$(top_srcdir)/include

JUL_jar_file = "$(builddir)/../../liblttng-ust-jul/liblttng-ust-jul.jar"

default: all

all: JULTest.class

clean-local:
	rm -f *.class

JULTest.class: JULTest.java
	$(JCC) -cp $(JUL_jar_file) -d "$(builddir)" "$(srcdir)/JULTest.java"

endif
