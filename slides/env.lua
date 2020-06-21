function Div(el)
  if el.classes:includes("tex-multicols") then
    return
      { pandoc.RawBlock("latex", "\\begin{multicols}{2}")
      , el
      , pandoc.RawBlock("latex", "\\end{multicols}")
      }
  end
  local kls, _ = el.classes:find_if(function (s) return string.match(s, "^tex%-") end)
  if kls then
    local texkls = kls:gsub("^tex%-", "", 1)
    if texkls then
      return
        { pandoc.RawBlock("latex", "\\begin{" .. texkls .. "}")
        , el
        , pandoc.RawBlock("latex", "\\end{" .. texkls ..  "}")
        }
    end
  end
end
