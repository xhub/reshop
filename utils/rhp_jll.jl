using Pkg
using Pkg.Artifacts
import Pkg.BinaryPlatforms
using SHA

###########################################################
# Start of config
###########################################################
name = "ReSHOP"
version = ENV["RHP_VER"]
jll_dir = ENV["JLL_DIR"]

url = "http://reshop.eu/julia/"
files = readdir(jll_dir, join=true)
artifact_toml = joinpath(jll_dir, "Artifacts.toml")

prefix = "$name.v$(version)"
postfix = "tar.gz"


###########################################################
# End of config
###########################################################


for f in files
	f_hash = create_artifact() do artifact_path
		run(`tar xvf $(f) -C $(artifact_path)`)
	end

	d_hash = ""
	open(f) do ff
		d_hash = bytes2hex(sha2_256(ff))
	end
	triplet_str = replace(replace(basename(f), "."*postfix => ""), prefix*"." => "")
	plat = Pkg.BinaryPlatforms.platform_key_abi(triplet_str)

	bind_artifact!(artifact_toml, name, f_hash, platform=plat, download_info=[(url*basename(f), d_hash)])
end

