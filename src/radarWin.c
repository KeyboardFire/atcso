#include <stdlib.h>
#include <string.h>  // WHY OH WHY is memmove defined in here
#include "radarWin.h"


#define drawBeacon(b, i) do { \
        mvwaddch(radarWin, (b).y, 2 * (b).x, '*'); \
        waddch(radarWin, '0' + (i)); \
    } while (0)
#define drawAirport(a, i) do { \
        mvwaddch(radarWin, (a).xy.y, 2 * (a).xy.x, "^_>_v_<_"[(a).dir]); \
        waddch(radarWin, '0' + (i)); \
    } while (0)


/**
 * Creates the radar window, the biggest one that has all the planes and stuff.
 */
WINDOW *createRadarWin(AtcsoData *data) {
    WINDOW *radarWin = newwin(21, 60, 0, 0);

    // draw the outline
    for (int i = 0; i < 59; ++i) waddch(radarWin, '-');
    waddch(radarWin, ' ');
    for (int i = 0; i < 19; ++i) {
        waddstr(radarWin, "| ");
        for (int j = 0; j < 28; ++j) waddstr(radarWin, ". ");
        waddstr(radarWin, "| ");
    }
    for (int i = 0; i < 59; ++i) waddch(radarWin, '-');
    waddch(radarWin, ' ');

    // add the exits
    XY *xy = data->exits;
    for (int i = 0; !isNull(*xy); ++xy, ++i) {
        mvwaddch(radarWin, xy->y, 2 * xy->x, '0' + i);
    }

    // add the beacons
    xy = data->beacons;
    for (int i = 0; !isNull(*xy); ++xy, ++i) {
        drawBeacon(*xy, i);
    }

    // add the airports
    Airport *airport = data->airports;
    for (int i = 0; !isNull(airport->xy); ++airport, ++i) {
        drawAirport(*airport, i);
    }

    updateRadarWin(data, radarWin);

    return radarWin;
}


/**
 * Updates and refreshes the radar window.
 */
bool updateRadarWin(AtcsoData *data, WINDOW *radarWin) {
    int nPlanes = 0;
    for (Plane *p = data->planes; !isNull(p->xy); ++p, ++nPlanes) {
        // redraw whatever the plane just flew over
        mvwaddstr(radarWin, p->xy.y, 2 * p->xy.x, ". ");
        int bIdx = 0;
        for (XY *b = data->beacons; !isNull(*b); ++b, ++bIdx) {
            if (eq(*b, p->xy)) drawBeacon(*b, bIdx);
        }
        int aIdx = 0;
        for (Airport *a = data->airports; !isNull(a->xy); ++a, ++aIdx) {
            if (eq(a->xy, p->xy)) drawAirport(*a, aIdx);
        }
    }

    int pIdx = 0;
    for (Plane *p = data->planes; !isNull(p->xy); ++p, ++pIdx) {
        // update altitidue
        if (p->targetAltitude > p->altitude) ++p->altitude;
        if (p->targetAltitude < p->altitude) --p->altitude;

        // update direction
        if (p->dir != p->targetDir) {
            // how this algorithm works: we want to normalize p->dir to 0, so
            // we can compare it to p->targetDir without "wrap-around." to
            // change p->dir to 0, we add -p->dir. therefore, the "normalized"
            // value of p->targetDir is p->targetDir-p->dir modulo 8.
            // unfortunately, in C, % is *remainder*, not modulo. so we had to
            // "manually" implement a specialized version in a #define above.
            bool clockwise;
            if (p->targetDir == CIRCLE_RIGHT) clockwise = true;
            else if (p->targetDir == CIRCLE_LEFT) clockwise = false;
            else clockwise = negmod((int)p->targetDir - (int)p->dir, 8) <= 4;

            if (clockwise) {
                for (int i = 0; (i < 2) && (p->dir != p->targetDir); ++i) {
                    p->dir = (p->dir + 1) % 8;
                }
            } else {
                for (int i = 0; (i < 2) && (p->dir != p->targetDir); ++i) {
                    p->dir = negmod((int)p->dir - 1, 8);
                }
            }
        }

        // update position
        p->xy.y += dy(p->dir);
        p->xy.x += dx(p->dir);

        // did the plane exit/crash?
        if (p->xy.y == 0 || p->xy.y == 20 || p->xy.x == 0 || p->xy.x == 29) {
            if (p->altitude != 9) return true;  // game over
            int exitNum = 0;
            for (XY *exit = data->exits; !isNull(*exit); ++exit, ++exitNum) {
                if (eq(*exit, p->xy)) {
                    // woohoo, plane exited!
                    if (p->destType == 'E' && p->dest == exitNum) {
                        // remove plane
                        goto exited;
                    } else {
                        return true;  // wrong exit, game over
                    }
                }
            }
            return true;  // plane didn't hit an exit; game over
        }

        // did the plane crash/land?
        if (p->altitude == 0) {
            int apNum = 0;
            for (Airport *ap = data->airports; !isNull(ap->xy); ++ap, ++apNum) {
                if (eq(ap->xy, p->xy)) {
                    // plane landed (hopefully correctly)
                    if (p->destType == 'A' && p->dest == apNum) {
                        if (p->dir == ap->dir) {
                            // remove plane
                            goto exited;
                        } else {
                            return true;  // wrong direction; game over
                        }
                    } else {
                        return true;  // wrong airport; game over
                    }
                }
            }
            return true;  // plane didn't land at an airport; game over
        }

        // draw the plane
        wattron(radarWin, A_REVERSE);
        mvwaddch(radarWin, p->xy.y, 2 * p->xy.x, p->name);
        waddch(radarWin, '0' + p->altitude);
        wattroff(radarWin, A_REVERSE);
        continue;

        exited:
        // remove plane
        memmove(data->planes + pIdx, data->planes + pIdx + 1,
                sizeof(Plane) * (nPlanes - pIdx));
        data->planes = realloc(data->planes, nPlanes * sizeof(Plane));

        // first find what index that element had in planesSorted
        int sortedIdx;
        for (int i = 0; i < nPlanes; ++i) {  // does not include "NUL" term.
            if (data->planesSorted[i] == pIdx) {
                sortedIdx = i;
                break;
            }
        }

        // remove index
        memmove(data->planesSorted + sortedIdx,
                data->planesSorted + sortedIdx + 1,
                sizeof(int) * (nPlanes - sortedIdx));
        data->planesSorted = realloc(data->planesSorted,
                nPlanes * sizeof(int));

        // shift all indeces after this down one
        for (int i = 0; i < nPlanes; ++i) {  // includes "NUL" term.
            if (data->planesSorted[i] > pIdx) {
                --data->planesSorted[i];
            }
        }

        ++data->score;
        --nPlanes;
        --pIdx;
        p = data->planes + pIdx;
    }

    // do we create a new plane?
    data->newPlaneCounter += (double)rand() / RAND_MAX;
    if (data->newPlaneCounter > data->newPlaneRate) {
        int nExits = 0;
        for (XY *exit = data->exits; !isNull(*exit); ++exit, ++nExits);
        int nAirports = 0;
        for (Airport *ap = data->airports; !isNull(ap->xy); ++ap, ++nAirports);

        // determine entry and destination
        XY entryCoords = data->exits[rand() % nExits];
        int dy = 0, dx = 0;
        if (entryCoords.y == 0)  { entryCoords.y = 1;  dy = 1;  }
        if (entryCoords.y == 20) { entryCoords.y = 19; dy = -1; }
        if (entryCoords.x == 0)  { entryCoords.x = 1;  dx = 1;  }
        if (entryCoords.x == 29) { entryCoords.x = 28; dx = -1; }
        Direction entryDir = fromdyx(dy, dx);
        int destIdx = rand() % (nExits + nAirports);

        // add plane
        data->planes = realloc(data->planes, (nPlanes + 2) * sizeof(Plane));
        data->planes[nPlanes] = (Plane) {entryCoords, data->nextLetter, 7, 7,
                entryDir, entryDir, destIdx >= nExits ? 'A' : 'E',
                destIdx >= nExits ? destIdx - nExits : destIdx};
        data->planes[nPlanes + 1] = (Plane) {{-1, -1}, 0, 0, 0, 0, 0, 0, 0};

        // add index of plane (we'll re-sort later)
        data->planesSorted = realloc(data->planesSorted,
                (nPlanes + 2) * sizeof(int));
        data->planesSorted[nPlanes] = nPlanes;
        data->planesSorted[nPlanes + 1] = nPlanes + 1;

        ++data->nextLetter;
        if (data->nextLetter > 'z') data->nextLetter = 'a';

        // draw the plane
        wattron(radarWin, A_REVERSE);
        mvwaddch(radarWin, data->planes[nPlanes].xy.y,
                2 * data->planes[nPlanes].xy.x,
                data->planes[nPlanes].name);
        waddch(radarWin, '0' + data->planes[nPlanes].altitude);
        wattroff(radarWin, A_REVERSE);

        // update counters
        data->newPlaneCounter -= data->newPlaneRate;
        ++nPlanes;
    }

    // refresh before doing collision detection so that user can actually see
    // what collided
    wrefresh(radarWin);

    // re-sort planesSorted to prepare to determine any collisions
    // we use insertion sort here because planesSorted is already almost fully
    //   sorted. a quicksort would be slower.
    for (int i = 1; i < nPlanes; ++i) {
        int j = i;
        int tmp = data->planesSorted[j];
        for (; j > 0 && (data->planes[tmp].xy.x <
                    data->planes[data->planesSorted[j - 1]].xy.x); --j) {
            data->planesSorted[j] = data->planesSorted[j - 1];
        }
        data->planesSorted[j] = tmp;
    }

    // find collisions between planes using planesSorted
    for (int i = 0; i < nPlanes - 1; ++i) {
        Plane *p1 = data->planes + data->planesSorted[i];
        for (int j = i + 1; j < nPlanes; ++j) {
            Plane *p2 = data->planes + data->planesSorted[j];
            if (p1->xy.x >= (p2->xy.x - 1)) {
                if (abs(p1->xy.y - p2->xy.y) <= 1 &&
                        abs(p1->altitude - p2->altitude) <= 1) {
                    return true;  // collision!
                }
            } else break;
        }
    }

    return false;
}
