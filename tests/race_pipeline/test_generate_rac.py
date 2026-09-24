import pytest

from generate_rac import apply_entity_names, generate_rac, load_names
from tests.race_pipeline.conftest import REPO, SCRIPTS

RAC = """name "Plastic Humans - Red"
author "X"
<Units>
  count 1
  <Unit 0>
    id "footman"
    name "Footman"
  </Unit 0>
</Units>
<Buildings>
  <Building 0>
    id "castle"
    name "Castle"
    <Products>
      <Product 0>
        product "footman"
      </Product>
    </Products>
  </Building 0>
</Buildings>
"""


def test_renames_units_and_buildings_by_id():
    out = apply_entity_names(RAC, {"footman": "Grunt", "castle": "Great Hall"})
    assert 'name "Grunt"' in out
    assert 'name "Great Hall"' in out
    assert 'name "Plastic Humans - Red"' in out  # header untouched
    assert 'product "footman"' in out            # references untouched


def test_unknown_id_raises():
    with pytest.raises(ValueError, match="troll"):
        apply_entity_names(RAC, {"troll": "Troll"})


def test_real_human_rac_all_orc_names_apply(tmp_path):
    names = load_names(SCRIPTS / "orc_entities.json")
    assert len(names) == 16
    out = tmp_path / "orc.rac"
    generate_rac(REPO / "races/human-red/human-red.rac", out,
                 "Plastic Orcs - Red", "Dark Oberon orc mod", names)
    text = out.read_text(encoding="latin-1")
    assert 'name "Plastic Orcs - Red"' in text
    assert 'name "Grunt"' in text and 'name "Footman"' not in text
    # nothing but names/header changed
    src = (REPO / "races/human-red/human-red.rac").read_text(encoding="latin-1")
    changed = [(a, b) for a, b in zip(src.splitlines(), text.splitlines()) if a != b]
    assert all(a.strip().startswith(("name", "author")) for a, _ in changed)
    assert len(src.splitlines()) == len(text.splitlines())
