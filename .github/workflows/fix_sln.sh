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
	GlobalSection(ProjectConfigurationPlatforms) = postSolution
EOF
while read -r line; do
	[ "${line:0:8}" != "Project(" ] && continue
	id="$(echo "${line}"|cut -d',' -f 3)"
	id="${id:2:-2}"
	for cfg in "Debug" "Release"; do
		for param in "ActiveCfg" "Build.0"; do
			echo "		${id}.${cfg}|Win32.${param} = ${cfg}|Win32"
		done
	done
done | crlf
crlf <<EOF
	EndGlobalSection
	GlobalSection(SolutionProperties) = preSolution
		HideSolutionNode = FALSE
	EndGlobalSection
EndGlobal
EOF
