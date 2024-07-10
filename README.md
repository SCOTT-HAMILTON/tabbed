<p align="center">
    <img width="400" alt="Tabbed Logo" src="https://tools.suckless.org/tabbed/tabbed.png">
</p>

<p align="center">
      <a href="https://scott-hamilton.mit-license.org/"><img alt="MIT License" src="https://img.shields.io/badge/License-MIT-525252.svg?labelColor=292929&logo=creative%20commons&style=for-the-badge" /></a>
	  <a href="https://github.com/SCOTT-HAMILTON/tabbed/actions"><img alt="Build Status" src="https://img.shields.io/endpoint.svg?url=https%3A%2F%2Factions-badge.atrox.dev%2FSCOTT-HAMILTON%2Ftabbed%2Fbadge&style=for-the-badge" /></a>
	  <a href="https://www.codacy.com/gh/SCOTT-HAMILTON/tabbed/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=SCOTT-HAMILTON/tabbed&amp;utm_campaign=Badge_Grade"><img alt="Code Quality" src="https://img.shields.io/codacy/grade/e3cfa183b64543df926a400c1fd0ac4c?style=for-the-badge&logo=codacy" /></a>
	  <a href="https://www.codacy.com/gh/SCOTT-HAMILTON/tabbed/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=SCOTT-HAMILTON/tabbed&amp;utm_campaign=Badge_Grade"><img alt="Code Quality" src="https://img.shields.io/codacy/coverage/e3cfa183b64543df926a400c1fd0ac4c?logo=codacy&style=for-the-badge" /></a>
</p>

<h1 align="center">Fork of Tabbed - The tab interface for application supporting Xembed </h1>
<h3 align="center">Checkout the original project <a href="https://tools.suckless.org/tabbed/">tabbed</a> and the alacritty fork needed to make this work <a href="https://github.com/SCOTT-HAMILTON/alacritty">alacritty</a></h3>

<p align="center">
  <img width="600"
       alt="Tabbed-Alacritty - Demo of directory following"
       src="https://user-images.githubusercontent.com/24496705/114284799-b6cfce80-9a52-11eb-91e6-98336eb92a7f.gif">
</p>

## About

This project is a fork of tabbed. (A name needs to be found tho).
It enables terminal emulators like alacritty to follow the previous working directory when opening a new tab.
For now, the only patched terminal to support this protocol is alacritty, see my fork : [https://github.com/SCOTT-HAMILTON/alacritty](https://github.com/SCOTT-HAMILTON/alacritty)

## How to test it right now

A nix shell is configured so that you can get this setup running in a few commands.
This shell builds the tabbed fork and this alacritty fork.

 1. First install nix see [https://nixos.org/guides/install-nix.html](https://nixos.org/guides/install-nix.html)
I higly recommand you to check out the above link but normally this command should be enough : 
```shell_session
 $ sh <(curl -L https://nixos.org/nix/install) --daemon
```
 2. Navigate to this repo
```shell_session
 $ cd ~/path/to/where/you/cloned/my/tabbed/fork
```
 3. enter the nix shell : 
```shell_session
 $ nix-shell
```
 4. (in the nix shell) build the alacritty fork : 
```shell_session
 $ cargo build
```
 5. (still in the nix shell) run the tabbed alacritty : 
```shell_session
 $ tabbed -cr 2 -w "--xembed-tcp-port" ./target/debug/alacritty --embed ""
```
**Bonus for hackers**  
6. (still in the nix shell) run the tabbed alacritty and put debug logs in a separate file : 
```shell_session
 $ tabbed -cr 2 -w "--xembed-tcp-port" ./target/debug/alacritty --embed "" 2>&1 | ./filter_output.pl 'debug-' debug_logs /dev/stdout 2>&1
```

## How does it work

[![Flowchart Diagram](https://mermaid.ink/img/eyJjb2RlIjoiZ3JhcGggVERcbiAgICBBW1RhYmJlZCBwYXJlbnQgcHJvY2Vzc11cbiAgICBcbiAgICBBIC0tPnxYSUQgMXwgVEMxW1RhYmJlZCBDbGllbnQgMV1cbiAgICBBIC0tPnxYSUQgMnwgVEMyW1RhYmJlZCBDbGllbnQgMl1cbiAgICBBIC0tPnxYSUQgM3wgVEMzW1RhYmJlZCBDbGllbnQgM11cbiAgICBUQzEgLS0-IEExW0FsYWNyaXR0eSB0YWIgMV1cbiAgICBUQzIgLS0-IEEyW0FsYWNyaXR0eSB0YWIgMl1cbiAgICBUQzMgLS0-IEEzW0FsYWNyaXR0eSB0YWIgM11cbiAgICAiLCJtZXJtYWlkIjp7InRoZW1lIjoiZGFyayJ9LCJ1cGRhdGVFZGl0b3IiOmZhbHNlfQ)](https://mermaid-js.github.io/mermaid-live-editor/#/edit/eyJjb2RlIjoiZ3JhcGggVERcbiAgICBBW1RhYmJlZCBwYXJlbnQgcHJvY2Vzc11cbiAgICBcbiAgICBBIC0tPnxYSUQgMXwgVEMxW1RhYmJlZCBDbGllbnQgMV1cbiAgICBBIC0tPnxYSUQgMnwgVEMyW1RhYmJlZCBDbGllbnQgMl1cbiAgICBBIC0tPnxYSUQgM3wgVEMzW1RhYmJlZCBDbGllbnQgM11cbiAgICBUQzEgLS0-IEExW0FsYWNyaXR0eSB0YWIgMV1cbiAgICBUQzIgLS0-IEEyW0FsYWNyaXR0eSB0YWIgMl1cbiAgICBUQzMgLS0-IEEzW0FsYWNyaXR0eSB0YWIgM11cbiAgICAiLCJtZXJtYWlkIjp7InRoZW1lIjoiZGFyayJ9LCJ1cGRhdGVFZGl0b3IiOmZhbHNlfQ)

When spawning a new alacritty window, tabbed also forks a child process that will communicate with this alacritty window through ZeroMQ Rep/Req sockets (Req->tabbed, Rep->the client).
This allows non-blocking bidirectionnal communications between the child process and the alacritty window.
**A child is referred to as a client in tabbed**

Each tabbed client is identified by an XID, which is the X11 Identifier of the alacritty window it's responsible of, (cf [https://metacpan.org/pod/X11::Xlib#DESCRIPTION](https://metacpan.org/pod/X11::Xlib#DESCRIPTION))

### Authentification Step 

The tabbed client doesn't know its window's XID when spawned, it needs to ask for it.
[![Tabbed client asking XID from alacritty window](https://mermaid.ink/img/eyJjb2RlIjoic2VxdWVuY2VEaWFncmFtXG4gICAgVGFiYmVkIENsaWVudCAxLT4-K0FsYWNyaXR0eSBUYWIgMTogSGVsbG8gQWxhY3JpdHR5LCB3aGF0J3MgeW91ciBYMTEgV2luZG93IElkZW50aWZpZXIgP1xuICAgIEFsYWNyaXR0eSBUYWIgMS0tPj4tVGFiYmVkIENsaWVudCAxOiBIaSwgbXkgWElEIGlzIDEzODQxMjAzNC5cbiAgICAgICAgICAgICIsIm1lcm1haWQiOnsidGhlbWUiOiJkZWZhdWx0In0sInVwZGF0ZUVkaXRvciI6ZmFsc2V9)](https://mermaid-js.github.io/mermaid-live-editor/#/edit/eyJjb2RlIjoic2VxdWVuY2VEaWFncmFtXG4gICAgVGFiYmVkIENsaWVudCAxLT4-K0FsYWNyaXR0eSBUYWIgMTogSGVsbG8gQWxhY3JpdHR5LCB3aGF0J3MgeW91ciBYMTEgV2luZG93IElkZW50aWZpZXIgP1xuICAgIEFsYWNyaXR0eSBUYWIgMS0tPj4tVGFiYmVkIENsaWVudCAxOiBIaSwgbXkgWElEIGlzIDEzODQxMjAzNC5cbiAgICAgICAgICAgICIsIm1lcm1haWQiOnsidGhlbWUiOiJkZWZhdWx0In0sInVwZGF0ZUVkaXRvciI6ZmFsc2V9)

The messages involved are : 
`XID?` and `XID:138412034`

### Loop

Now that the client is authentified, in other words, now that it knows its associated window's XID, it can enter the following two state loops :  

#### The communication loop

[![](https://mermaid.ink/img/eyJjb2RlIjoic3RhdGVEaWFncmFtLXYyXG4gICAgc3RhdGUgXCJEaWQgSSByZWNlaXZlIGEgbWVzc2FnZSA_XCIgYXMgVDFcbiAgICBzdGF0ZSBcIklmIGl0J3MgYSBQV0QgQW5zd2VyLCBzYXZlIGl0IGZvciBsYXRlclwiIGFzIFNcblxuICAgIFsqXSAtLT4gVDFcbiAgICBUMSAtLT4gTm9cbiAgICBObyAtLT4gVDFcbiAgICBUMSAtLT4gWWVzXG4gICAgWWVzIC0tPiBTXG4gICAgUyAtLT4gVDFcbiIsIm1lcm1haWQiOnsidGhlbWUiOiJkZWZhdWx0In0sInVwZGF0ZUVkaXRvciI6ZmFsc2V9)](https://mermaid-js.github.io/mermaid-live-editor/#/edit/eyJjb2RlIjoic3RhdGVEaWFncmFtLXYyXG4gICAgc3RhdGUgXCJEaWQgSSByZWNlaXZlIGEgbWVzc2FnZSA_XCIgYXMgVDFcbiAgICBzdGF0ZSBcIklmIGl0J3MgYSBQV0QgQW5zd2VyLCBzYXZlIGl0IGZvciBsYXRlclwiIGFzIFNcblxuICAgIFsqXSAtLT4gVDFcbiAgICBUMSAtLT4gTm9cbiAgICBObyAtLT4gVDFcbiAgICBUMSAtLT4gWWVzXG4gICAgWWVzIC0tPiBTXG4gICAgUyAtLT4gVDFcbiIsIm1lcm1haWQiOnsidGhlbWUiOiJkZWZhdWx0In0sInVwZGF0ZUVkaXRvciI6ZmFsc2V9)

The **PWD** is the shell's working directory of the current focused window.
It changes each time the user executes a `cd /somewhere/` command.  
The message involved is : `PWD:/home/user`

#### The logic loop

[![](https://mermaid.ink/img/eyJjb2RlIjoic3RhdGVEaWFncmFtLXYyXG4gICAgc3RhdGUgXCJBbSBJIHRoZSBmb2N1c2VkIHRhYiA_XCIgYXMgVDFcbiAgICBzdGF0ZSBcIlNsZWVwIE1vZGVcIiBhcyBTXG4gICAgc3RhdGUgXCJUdXJibyBNb2RlXCIgYXMgVFxuICAgIHN0YXRlIFwiVHJhbnNtaXQgdGhlIHNhdmVkIHNoZWxsIFBXRCB0byB0YWJiZWQgcGFyZW50IHByb2Nlc3MgaWYgYW55IHdhcyByZWNlaXZlZFwiIGFzIFBcbiAgICBzdGF0ZSBcIkFzayB3aW5kb3cgZm9yIHNoZWxsIFBXRFwiIGFzIEExXG5cblxuICAgIFsqXSAtLT4gVDFcbiAgICBUMSAtLT4gTm9cbiAgICBObyAtLT4gU1xuICAgIFMgLS0-IFQxXG4gICAgVDEgLS0-IFllc1xuICAgIFllcyAtLT4gVFxuICAgIFQgLS0-IFBcbiAgICBQIC0tPiBBMVxuICAgIEExIC0tPiBUMSAiLCJtZXJtYWlkIjp7InRoZW1lIjoiZGVmYXVsdCJ9LCJ1cGRhdGVFZGl0b3IiOmZhbHNlfQ)](https://mermaid-js.github.io/mermaid-live-editor/#/edit/eyJjb2RlIjoic3RhdGVEaWFncmFtLXYyXG4gICAgc3RhdGUgXCJBbSBJIHRoZSBmb2N1c2VkIHRhYiA_XCIgYXMgVDFcbiAgICBzdGF0ZSBcIlNsZWVwIE1vZGVcIiBhcyBTXG4gICAgc3RhdGUgXCJUdXJibyBNb2RlXCIgYXMgVFxuICAgIHN0YXRlIFwiVHJhbnNtaXQgdGhlIHNhdmVkIHNoZWxsIFBXRCB0byB0YWJiZWQgcGFyZW50IHByb2Nlc3MgaWYgYW55IHdhcyByZWNlaXZlZFwiIGFzIFBcbiAgICBzdGF0ZSBcIkFzayB3aW5kb3cgZm9yIHNoZWxsIFBXRFwiIGFzIEExXG5cblxuICAgIFsqXSAtLT4gVDFcbiAgICBUMSAtLT4gTm9cbiAgICBObyAtLT4gU1xuICAgIFMgLS0-IFQxXG4gICAgVDEgLS0-IFllc1xuICAgIFllcyAtLT4gVFxuICAgIFQgLS0-IFBcbiAgICBQIC0tPiBBMVxuICAgIEExIC0tPiBUMSAiLCJtZXJtYWlkIjp7InRoZW1lIjoiZGVmYXVsdCJ9LCJ1cGRhdGVFZGl0b3IiOmZhbHNlfQ)

When entering sleep mode, the tabbed client also sends a message informing the window that it should also enter the sleep mode, same goes to the turbo mode.
These modes are critical for limiting CPU usage.

Since the user can change the working directory at anytime, the client has to ask for it constantly.
And so it needs to transmit the flow of answers to the tabbed parent process.
This is the **key system** that allows it to follow the working directories.
Because the tabbed parent process always knows the PWD of the currently focused window's shell, it can spawn a new one at the good location.

The messages involved are : `sleep`, `turbo` and `PWD?`

## License
Tabbed is released under the [MIT/X Consortium License](https://git.suckless.org/tabbed/file/LICENSE.html)
This few patches are released under the [MIT License](https://scott-hamilton.mit-license.org/)

**References that helped**
  - [qubes-os markdown conventions] : <https://www.qubes-os.org/doc/doc-guidelines/#markdown-conventions>
  - [Linux man pages] : <https://linux.die.net/man/>
  - [TcpStream rust doc] : <https://docs.rs/mio/0.5.1/mio/tcp/struct.TcpStream.html>
  - [mermaid-js documentation] : <https://mermaid-js.github.io/mermaid/#/stateDiagram>

# (These are reference links used in the body of this note and get stripped out when the markdown processor does its job. There is no need to format nicely because it shouldn't be seen. Thanks SO - http://stackoverflow.com/questions/4823468/store-comments-in-markdown-syntax)

   [qubes-os markdown conventions]: <https://www.qubes-os.org/doc/doc-guidelines/#markdown-conventions/>
   [linux man pages]: <https://linux.die.net/man/>
   [tcpstream rust doc]: <https://docs.rs/mio/0.5.1/mio/tcp/struct.TcpStream.html>
   [mermaid-js documentation]: <https://mermaid-js.github.io/mermaid/#/stateDiagram>
