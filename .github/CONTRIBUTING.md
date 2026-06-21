This file details how to contribute to the Mapbase project on GitHub:
	https://github.com/mapbase-source/source-sdk-2013

For the original Source SDK 2013 contribution guidelines, click here:
	https://github.com/ValveSoftware/source-sdk-2013/blob/master/CONTRIBUTING

---

Due to the high number of Source mods which draw from Mapbase, the Mapbase repository needs to maintain
certain standards of compatibility and attribution, but it is still an open-source project and it will always
be open to contributions. Do not let these guidelines intimidate you, but please keep them in mind.

Whenever you contribute to the Mapbase repository, your contribution will potentially be deployed to all
projects utilizing Mapbase, including many high-profile mods. Contributions can also end up being available
in HL2, HL2:DM, *and* TF2 mods if the contributions are not obviously exclusive to one of them.

---

# Branches

The Mapbase repository has two main branches: `master` and `develop`.

`master` represents the last stable release, while `develop` represents the upcoming release. There may be other
branches to represent specific efforts, such as `mapbase-mp-2025` for Mapbase's port to the TF2 SDK.

Contributions should almost always either target the `develop` branch or one of these specialized branches. Do not
target `master` unless you have a specific reason to.

---

# Guidelines

All contributions are subject to the following guidelines:

## Purpose

 * A contribution must be aligned with Mapbase's goals and priorities and should not be "subjective"
   or related to a specific mod or type of mod. For example, fixing an existing issue or adding a
   new tool for mappers to use is usually fine, but adding a new custom weapon with its own assets
   is usually not fit for Mapbase.
   
## Attribution
   
 * All content in a contribution must be either already legally open-source or done with the
   full permission of the content's original creator(s). If a license is involved, the contributor
   should ensure Mapbase conforms to its terms.
    * **NOTE:** Due to concerns with mods which do not wish to be open-source, content using GPL licenses (or any
	  license with similar open-source requirements) are currently not allowed to be distributed with Mapbase.
	  Contributions which can draw from them without actually distributing the licensed content may be excepted
	  from this rule.
   
 * If you are contributing a file you created yourself specifically for Mapbase, you are required to
   use the custom "Mapbase - Source 2013" header used in other Mapbase files as of Mapbase v5.0.
   You are encouraged to append an "Author(s)" part to that header in your file in order to clarify who wrote it.
   
 * Contributions which were clearly written or described using generative AI are not allowed and will be rejected.
   This includes code as well as pull request descriptions.
    * Using AI in a minor capacity (e.g. technical support, proofreading) is acceptable as long as the PR is otherwise
	  your own work.
   
 * Do not modify the README to add attribution for your contribution. That is handled by Mapbase's maintainers.
   
## Compatibility
   
 * Contributions must not break existing maps/content or interfere with them in a negative or non-objective way.
    * Contributions should also avoid removing content which might be referenced or used by other mods.
 
 * New spawnflags for existing entities will be discouraged and/or subject to increased scrutiny.
 
 * Contributions which break pre-existing saves may need to wait until the next major Mapbase update before they
   are merged. (for example, the '8' in 'v8.0' signifies a major update)
 
 * Signature matching against engine binaries is not an acceptable workaround for calling engine functions.
 
 * Contributions should be compatible with Mapbase's platforms and build tools. See the 'Checks' section further
   below for more details.

## Conformity

 * Pull requests should be made with branches which are already derived from Mapbase, rather than stock Source 2013.
   This is important for the pull request to initiate GitHub Actions, which is covered in the 'Checks' section below.
   
 * Code contributions which modify or add onto existing code should generally match its syntax and shouldn't
   change the spacing unless necessary.
   
 * Code contributions are *not* obliged to follow Mapbase's preprocessor conventions (e.g. #ifdef MAPBASE),
   although following them is usually acceptable.

---
   
Contributions which do not follow these guidelines cannot be accepted into Mapbase. If there is an issue with your pull request,
a reviewer may request changes. There is normally no other penalty for violating these guidelines, although repeatedly
attempting to contribute prohibited content can lead to being blocked from contributing.

---

# Reviews

A pull request must have an approving review from at least one of Mapbase's collaborators. "Collaborators" are users with permission
to approve other people's PRs, although any GitHub user can review or leave comments on a pull request.

After your pull request is approved, you are advised to avoid making further changes to it. Too many changes may necessitate a new review.

Please note that we have a very small number of collaborators, and depending on the circumstances, it may be some time before we get around to
reviewing your pull request. If you are interested in helping out, then you can ask for collaborator permissions with [this short 4-question form](https://docs.google.com/forms/d/e/1FAIpQLSeLfgw9UlHXuM_9mWrhHv7Z5i3SXnEHaufG5Kb25gT0nXZcfg/viewform?usp=sf_link).
You can also see [Reviewing Mapbase pull requests](https://github.com/mapbase-source/source-sdk-2013/wiki/Reviewing-Mapbase-pull-requests)
for more information on how to review a pull request in general.

---

# Checks

Mapbase uses GitHub Actions to help manage issues and pull requests. Some of these workflows build the code of incoming
contributions to make sure they compile properly. The code is compiled separately for Visual Studio 2022 and GCC/G++ 9 (Linux)
and on both Debug and Release configurations.

If these workflows fail, don't freak out! Accidents can happen frequently due to compiler syntax differences and conflicts
from other contributions. You can look at a failed workflow's log by clicking "Details", which will include the build's output
in the "Build" step(s). Any errors must be resolved by you and/or by code reviewers before a pull request can be merged.

---

# Post-Contribution

If your contribution is accepted, you may be listed in Mapbase's credits and the README's external content list:
	https://github.com/mapbase-source/source-sdk-2013/wiki/Mapbase-Credits#Contributors
	https://github.com/mapbase-source/source-sdk-2013/blob/master/README
	
You may also receive the "Contributor" or "Major Contributor" role on Mapbase's Discord server if you are
a member of it.

