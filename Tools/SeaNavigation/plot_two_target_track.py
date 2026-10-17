"""Plot the recorded physical loop against the supplied coastline and buffer."""
import json
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon, Patch, Circle

root=Path(__file__).resolve().parents[2]
read=lambda p:json.loads(p.read_text(encoding="utf8"))
run=read(root/"Docs/ShipEnemyNavigation/TWO_TARGET_PIE.json")
chart=read(root/"Content/SeaNavigation/halong_navigation.json")
land=read(root/"Data/SeaNavigation/halong_land.geojson")["features"][0]["geometry"]
buffered=read(root/"Data/SeaNavigation/halong_buffered_enu.geojson")["geometry"]
limits=(-650,1000,-2600,-650)
def en(ring):
    xy=np.radians(np.asarray(ring)[:,:2]);lon,lat=xy[:,0],xy[:,1]
    radius=6378137/np.sqrt(1-0.0066943799901413165*np.sin(lat)**2)
    ecef=np.column_stack((radius*np.cos(lat)*np.cos(lon),radius*np.cos(lat)*np.sin(lon),radius*(1-0.0066943799901413165)*np.sin(lat)))
    delta=ecef-np.asarray(chart["origin_ecef"])
    return np.column_stack((delta@np.asarray(chart["east_axis"]),delta@np.asarray(chart["north_axis"])))
fig,ax=plt.subplots(figsize=(8,9),layout="constrained")
ax.set_facecolor("#e8f3fa")
for geometry,color,convert,zorder in [(buffered,"#f4d6ba",False,1),(land,"#c0c7bf",True,2)]:
    polygons=geometry["coordinates"] if geometry["type"]=="MultiPolygon" else [geometry["coordinates"]]
    for polygon in polygons:
        ring=en(polygon[0]) if convert else np.asarray(polygon[0])[:,:2]
        if ring[:,0].max()<limits[0] or ring[:,0].min()>limits[1] or ring[:,1].max()<limits[2] or ring[:,1].min()>limits[3]:continue
        ax.add_patch(Polygon(ring,facecolor=color,edgecolor="#8e958e",linewidth=.35,zorder=zorder))
rows=run["rows"]
for leg,color,label in [(0,"#1264bd","Spawn → A"),(1,"#c86800","A → B"),(2,"#16854b","B → A")]:
    points=np.asarray([r["en_m"] for r in rows if r["completed_legs"]==leg])
    if len(points)>1:ax.plot(points[:,0],points[:,1],color=color,lw=2,label=label,zorder=5)
a,b=np.asarray(rows[0]["target_a_en"]),np.asarray(rows[0]["target_b_en"])
ax.plot([a[0],b[0]],[a[1],b[1]],"--",color="#6d6d6d",lw=1,label="Direct A–B line",zorder=3)
for name,point in [("A",a),("B",b)]:
    ax.add_patch(Circle(point,60,fill=False,edgecolor="#b01834",linestyle=":",linewidth=1,zorder=4))
    ax.scatter(*point,color="#b01834",edgecolor="white",s=90,zorder=6)
    ax.annotate(name,point,xytext=(-18,7),textcoords="offset points",fontsize=12,weight="bold",zorder=7)
start=rows[0]["en_m"]
ax.scatter(*start,marker="s",color="#1264bd",edgecolor="white",s=65,zorder=6)
ax.annotate("Spawn",start,xytext=(-50,-18),textcoords="offset points",fontsize=9)
ax.set(xlim=limits[:2],ylim=limits[2:],xlabel="Fixed chart East (m)",ylabel="Fixed chart North (m)",
    title="Recorded physical ship navigation: A → B → A")
ax.set_aspect("equal");ax.grid(alpha=.22,zorder=0)
handles,labels=ax.get_legend_handles_labels()
handles.extend([Patch(facecolor="#c0c7bf",label="OSM land"),Patch(facecolor="#f4d6ba",label="125 m coast exclusion")])
ax.legend(handles=handles,loc="upper right",fontsize=8,framealpha=.97)
ax.text(.02,.015,"Real PIE • 2× simulation time • 3 settled arrivals\nDotted circles: 60 m arrival radius\nNo recorded land/buffer crossing • Original saved targets",transform=ax.transAxes,fontsize=8,va="bottom",
    bbox=dict(facecolor="white",alpha=.92,edgecolor="none",pad=5),zorder=8)
out=root/"Docs/ShipEnemyNavigation/TWO_TARGET_TRACK.png"
fig.savefig(out,dpi=180);plt.close(fig)
print(out)
