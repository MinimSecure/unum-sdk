Contributing Guidelines
=======================

This document describes in general terms how to contribute to the Unum SDK.


Get started
-----------

Thanks for your interest in contributing to Unum SDK! We hope to make
contributing as simple as possible-- don't worry too much about protocol
around [opening an issue][1] or [Pull Request (aka PR)][2].

There is only one absolute requirement: **Treat everyone with respect!**


### Report a vulnerability

Contact us [directly by mail at labs@minim.co][6] to report security-related
or otherwise sensitive matters.

Encode your message with the [Minim Labs public key (0x5512FA98E0424563)][8],
if desired.


### Open an Issue

Don't sweat it, just [open an issue on our GitHub][1].

We have a few issues templates to help streamline issue reporting, but if none
of the templates fit your situation, feel free to improvise!


### Open a Pull Request

In general, follow these steps when opening a pull request:

1. [Fork the Unum SDK repository][3] on GitHub
2. Clone your fork to your local machine
   ```bash
   git clone git@github.com:YOURUSERNAME/unum-sdk
   cd unum-sdk
   ```
3. Check out a new branch -- name it anything!
   ```bash
   git checkout -b patch-1
   ```
4. Make your changes, commit, and push to your fork on GitHub
   ```bash
   git add -u
   git commit -m "Try to write a coherent commit message"
   git push origin patch-1
   ```
5. [Open a Pull Request][2] on the main Unum SDK GitHub repository


When opening a pull request, please consider:

- Every pull request must pass CI before being merged, and merges are 
  performed by rebase-and-squashing.
- Expect to be requested to make at least a couple of changes on your pull
  requests-- please take no offense at these requests! Code reviews can be
  high-tension and high-emotion, but we'll do our best to be diplomatic about
  our requests. Do not be afraid to disagree or ask for further explanation
  or justification, however!


Helpful things to include
-------------------------

Whether you're opening an issue or a PR, it is helpful for us to know:

- Platform you're building the SDK on (if applicable)
- Platform you're running Unum on
- Version of the Unum SDK you're using


Other things to be aware of
---------------------------

- Unum SDK is licensed under [Apache 2.0][4] and should not contain any GPL-
  licensed code. Any contributions are expected to be original works unless
  otherwise explicitly noted and attributed.
- Unum SDK is based on an internal Minim project and should remain structurally
  and technically compatible-- we don't recommend drastic changes without
  chatting with us first!
- If you have a negative experience, please call it out so that it can be
  addressed. We aim to never intentionally belittle, insult, or mock, but to
  err is human-- let us know so we can adjust our behavior.


## Happy Contributing!


Unum SDK is an open source project by [Minim][5]. [Send us mail][6] or [check
out our blog!][7]

[1]: https://github.com/MinimSecure/unum-sdk/issues/new
[2]: https://github.com/MinimSecure/unum-sdk/compare
[3]: https://github.com/MinimSecure/unum-sdk/fork
[4]: https://github.com/MinimSecure/unum-sdk/blob/master/LICENSE
[5]: https://www.minim.co
[6]: mailto:labs@minim.co
[7]: https://www.minim.co/blog
[8]: https://pgp.key-server.io/0x5512FA98E0424563