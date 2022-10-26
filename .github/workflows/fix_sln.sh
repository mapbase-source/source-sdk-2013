#!/bin/bash -eu

IFS=''

crlf() {
	while read -r line; do
		printf '%s\r\n' "${line}"
	done
}

crlf <<EOF
Global
	GlobalSection(SolutionConfigurationPlatforms) = preSolution
		Debug|Win32 = Debug|Win32
		Release|Win32 = Release|Win32
	EndGlobalSection
	GlobalSection(SolutionProperties) = preSolution
		HideSolutionNode = FALSE
	EndGlobalSection
EndGlobal
EOF
