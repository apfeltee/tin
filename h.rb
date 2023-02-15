
raw = File.read(ARGV.shift)

rx = /\btypedef\b\s*\b(?<type>(struct|enum|union))\s*(?<prename>\w+)?(?<body>\{.*?\})\s*(?<finalname>\w+)\s*;/m

results = raw.enum_for(:scan, rx).map{ Regexp.last_match  }

ttd = []
sources = []
begin
    results.each do |res|
        type = res["type"]
        pren = res["prename"]
        body = res["body"]
        finalname = res["finalname"]
        ttd.push(sprintf("typedef %s /**/%s %s;", type, finalname, finalname))
        sources.push(sprintf("%s %s\n%s;\n", type, finalname, body))
    end
  ttd.each do |l|
    puts(l)
  end
  sources.each do |l|
    puts(l)
  end
end
