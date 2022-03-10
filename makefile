ifeq ($(OS),Windows_NT)
	include makefileWindow
else
	include makefileLinux
endif