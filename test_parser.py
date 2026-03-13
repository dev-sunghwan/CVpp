import re

xml = """ateObjects></tt:Appearance></tt:Object><tt:Object ObjectId="8670"><tt:Appearance><tt:Shape><tt:BoundingBox left="219.0" top="455.0" right="298.0" bottom="500.0"/><tt:CenterOfGravity x="258.5" y="477.5"/></tt:Shape><tt:Class><tt:ClassCandidate><tt:Type>Vehical</tt:Type><tt:Likelihood>0.59</tt:Likelihood></tt:ClassCandidate><tt:Type Likelihood="0.59">Vehicle</tt:Type></tt:Class><tt:VehicleInfo><tt:Type Likelihood="0.59">Car</tt:Type></tt:VehicleInfo></tt:Appearance></tt:Object></tt:Frame></tt:VideoAnalytics></tt:MetadataStream>"""

obj_blocks = re.finditer(r"<tt:Object ObjectId=\"(.*?)\">(.*?)</tt:Object>", xml)
for match in obj_blocks:
    obj_id = match.group(1)
    content = match.group(2)
    bbx_match = re.search(r"<tt:BoundingBox\s+left=\"([0-9.-]+)\"\s+top=\"([0-9.-]+)\"\s+right=\"([0-9.-]+)\"\s+bottom=\"([0-9.-]+)\"", content)
    type_match = re.search(r"<tt:Type Likelihood=\"([0-9.-]+)\">([^<]+)</tt:Type>", content)
    if bbx_match and type_match:
        print(f"ID:{obj_id} Type:{type_match.group(2)} Prob:{type_match.group(1)} Box:{bbx_match.groups()}")
