#!/usr/bin/perl -w

use strict;
use File::Path;
use File::Spec;

# Set up absolute paths so this test can be run from anywhere
my ($volume_path, $directories_path, $file_path) = File::Spec->splitpath(File::Spec->rel2abs($0));
my $script_dir = defined($volume_path) && $volume_path ne "" ? File::Spec->catdir($volume_path, $directories_path) : $directories_path;

lipo_libs($script_dir, "release", "plugins");
lipo_libs($script_dir, "debug",   "plugins");

sub lipo_libs
{
	my $script_dir = shift;
	my $lib_folder = shift;
	my $plugin_folder = shift;

	chdir($script_dir);
	
	# Get a list of all the files
	chdir($lib_folder."_i386");
	my @dylib_files = <*.dylib>;
	chdir($plugin_folder);
	my @so_files = <*.so>;
	
	chdir($script_dir);
	
	# Make the folders for the build if they don't exist
	if (!(-e $lib_folder))
	{
		mkpath($lib_folder);
	}
	chdir($lib_folder);

	if (!(-e $plugin_folder))
	{
		mkpath($plugin_folder);
	}
	chdir($script_dir);

	# Kill any files that already exist
	foreach my $file (@dylib_files)
	{
		unlink(File::Spec->catdir($lib_folder, $file));
	}
	foreach my $file (@so_files)
	{
		unlink(File::Spec->catdir($lib_folder, $plugin_folder, $file));
	}
	
	# Lipo the files together
	foreach my $file (@dylib_files)
	{
		system("lipo -create ".File::Spec->catdir($lib_folder."_i386", $file)." ".File::Spec->catdir($lib_folder."_x86_64", $file)." -output ".File::Spec->catdir($lib_folder, $file));
	}
	
	foreach my $file (@so_files)
	{
		system("lipo -create ".File::Spec->catdir($lib_folder."_i386", $plugin_folder, $file)." ".File::Spec->catdir($lib_folder."_x86_64", $plugin_folder, $file)." -output ".File::Spec->catdir($lib_folder, $plugin_folder, $file));
	}
}
