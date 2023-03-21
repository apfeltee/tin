#!./run

class CGI
{
    constructor()
    {
    }

    beginHeaders()
    {
    }

    endHeaders()
    {
        STDOUT.write("\r\n")
    }

    sendHeader(k, v)
    {
        STDOUT.write("%s: %s\r\n".format(k, v))
    }
    
}


class HTMLGen
{
    constructor(out)
    {
        this.out = out
    }

    put(s)
    {
        this.out.write(s)
    }

    tag(name, attribs, fn)
    {
        if(attribs.length == 0)
        {
            this.put("<%s>".format(name))
        }
        else
        {
            var vi = 0;
            this.put("<%s ".format(name))
            for(var k in attribs)
            {
                vi++;
                this.put("%s=\"%s\"".format(k, attribs[k]))
                if((vi+1) < attribs.length)
                {
                    this.put(" ")
                }
            }
            this.put(">")
        }
        if(fn.class.name == "Function")
        {
            fn(this)
        }
        else
        {
            this.put(fn)
        }
        this.put("</%s>".format(name))
    }
    
}

class App
{
    constructor()
    {
        this.cgi = new CGI()
    }

    sendPage(ctype)
    {
        this.cgi.beginHeaders()
        this.cgi.sendHeader("Content-Type", ctype)
        this.cgi.endHeaders()
    }

    sendIndex()
    {
        this.sendPage("text/html")
        gh = new HTMLGen(STDOUT)
        gh.tag("html", {}, ()=>
        {
            gh.tag("head", {}, ()=>{})
            gh.tag("body", {}, ()=>
            {
                gh.tag("h1", {}, "files: ")
                gh.tag("ul", {}, ()=>
                {
                    for(var itm in Dir.read("."))
                    {
                        if((itm == ".") || (itm == ".."))
                        {
                            continue;
                        }
                        url = ("web.cgi/v?f=%s".format(itm));
                        gh.tag("li", {}, ()=>
                        {
                            gh.tag("a", {"href": url}, itm)
                        });
                        gh.put("\n");
                    }
                })
            })
        })
    }
}

function main()
{
    var app = new App()
    app.sendIndex()
}


main()