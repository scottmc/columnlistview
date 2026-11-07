# Simple Makefile for ColumnListViewDemo (run on Haiku).
#
#   make          build
#   make run      build and run
#   make clean    remove build output

NAME     = ColumnListViewDemo
SRCS     = ColumnListViewDemo.cpp
OBJS     = $(SRCS:.cpp=.o)

CXX      ?= g++
CXXFLAGS = -O2 -Wall -Wno-multichar \
           -I/boot/system/develop/headers/private/interface
# BColumnListView is not part of libbe; it lives in a static helper library.
LIBS     = -lcolumnlistview -lbe -lroot

$(NAME): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LIBS)
	mimeset -f $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(NAME)
	./$(NAME)

clean:
	rm -f $(OBJS) $(NAME)

.PHONY: run clean
